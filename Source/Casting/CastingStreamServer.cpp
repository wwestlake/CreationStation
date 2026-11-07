#include "CastingStreamServer.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <juce_core/juce_core.h>

namespace cs::casting
{

namespace
{
// Bounds on retained stream history, so a receiver that connects late (or reconnects) gets
// something to resync with, without the server's own memory use growing without limit for a
// long-running cast session. The already-connected, normal-case client is unaffected by this
// cap -- it keeps receiving every byte live, with nothing dropped, for as long as it keeps up.
constexpr size_t initSegmentCacheCap = 256 * 1024;       // generous for any fMP4 ftyp+moov+mvex.
constexpr size_t liveHistoryCapBytes = 32 * 1024 * 1024; // ~a minute or so at typical bitrates.
constexpr int outgoingQueueCapacity = 256;                // bounded per-client chunk backlog.

juce::String findLanAddress()
{
    juce::Array<juce::IPAddress> addresses;
    juce::IPAddress::findAllAddresses(addresses, false);
    for (auto& addr : addresses)
    {
        const auto s = addr.toString();
        if (s.startsWith("127.") || s.startsWith("169.254."))
            continue;
        const auto octets = juce::StringArray::fromTokens(s, ".", {});
        const bool isPrivate172 = octets.size() == 4 && octets[0] == "172"
                                    && octets[1].getIntValue() >= 16 && octets[1].getIntValue() <= 31;
        if (s.startsWith("10.") || s.startsWith("192.168.") || isPrivate172)
            return s;
    }
    // No private-range address found -- fall back to the first non-loopback address rather
    // than failing outright; still excludes loopback so a real external receiver has a chance.
    for (auto& addr : addresses)
    {
        const auto s = addr.toString();
        if (! s.startsWith("127."))
            return s;
    }
    return {};
}

juce::String makeSessionToken()
{
    return juce::Uuid().toString().removeCharacters("-").toLowerCase();
}
}

struct CastingStreamServer::Impl
{
    std::unique_ptr<juce::StreamingSocket> listener;
    std::thread acceptThread;
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> running { false };
    juce::String path;

    std::mutex historyMutex;
    std::vector<uint8_t> initSegment;
    std::deque<std::vector<uint8_t>> liveHistory;
    size_t liveHistoryBytes = 0;

    // Note: a Client's writer thread is only ever joined by CastingStreamServer::stop(), from
    // whatever thread calls it -- never by the Client itself, and the thread's own lambda never
    // captures a shared_ptr to its own Client (which would make a running thread keep itself
    // alive and risk joining itself from inside its own call stack on teardown). The thread
    // lambda below captures a raw Client* instead; `clients` is this object's sole owner.
    struct Client
    {
        std::unique_ptr<juce::StreamingSocket> socket;
        std::thread writer;
        std::atomic<bool> alive { true };
        std::mutex queueMutex;
        std::deque<std::vector<uint8_t>> outgoing;
        size_t droppedChunks = 0;
    };
    std::mutex clientsMutex;
    std::vector<std::shared_ptr<Client>> clients;

    ~Impl() { stop(); }

    juce::String start()
    {
        const auto address = findLanAddress();
        if (address.isEmpty())
            return {};

        for (int port = 45100; port <= 45200; ++port)
        {
            auto socket = std::make_unique<juce::StreamingSocket>();
            if (socket->createListener(port, address))
            {
                listener = std::move(socket);
                path = "/cast/" + makeSessionToken();
                stopRequested = false;
                running = true;
                acceptThread = std::thread([this] { acceptLoop(); });
                return "http://" + address + ":" + juce::String(port) + path;
            }
        }
        return {};
    }

    void acceptLoop()
    {
        while (! stopRequested.load())
        {
            auto* raw = listener->waitForNextConnection();
            if (raw == nullptr)
                continue; // timed out / interrupted; createListener sockets return null on close too.
            std::unique_ptr<juce::StreamingSocket> incoming(raw);
            handleConnection(std::move(incoming));
        }
    }

    void handleConnection(std::unique_ptr<juce::StreamingSocket> socket)
    {
        // Minimal request-line read: enough to validate method/path, nothing more -- this is a
        // single-purpose stream endpoint, not a general HTTP server.
        juce::String requestLine;
        char c = 0;
        for (int i = 0; i < 8192; ++i)
        {
            const auto n = socket->read(&c, 1, true);
            if (n <= 0 || c == '\n')
                break;
            if (c != '\r')
                requestLine += juce::String::charToString((juce::juce_wchar) c);
        }
        // Drain the rest of the headers without needing them.
        int blankLines = 0;
        char prev = 0;
        for (int i = 0; i < 65536 && blankLines < 1; ++i)
        {
            char ch = 0;
            if (socket->read(&ch, 1, true) <= 0)
                break;
            if (ch == '\n' && prev == '\n')
                blankLines = 1;
            prev = (ch == '\r') ? prev : ch;
        }

        const bool valid = requestLine.startsWith("GET " + path + " ");
        if (! valid)
        {
            static const char notFound[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
            socket->write(notFound, (int) sizeof(notFound) - 1);
            return; // socket destructs -> closes.
        }

        static const char headers[] =
            "HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nTransfer-Encoding: chunked\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
        if (socket->write(headers, (int) sizeof(headers) - 1) < 0)
            return;

        auto client = std::make_shared<Client>();
        client->socket = std::move(socket);
        {
            std::lock_guard<std::mutex> lock(historyMutex);
            if (! initSegment.empty())
                client->outgoing.push_back(initSegment);
            for (auto& chunk : liveHistory)
                client->outgoing.push_back(chunk);
        }
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back(client);
        }
        auto* rawClient = client.get();
        client->writer = std::thread([this, rawClient] { writeLoop(rawClient); });
    }

    // Takes a raw pointer deliberately -- see the note on Client above. `client` is guaranteed
    // to outlive this function: it stays in `clients` (keeping its shared_ptr refcount above
    // zero) until stop() has already joined this very thread and only then erases it.
    void writeLoop(Client* client)
    {
        while (client->alive.load() && ! stopRequested.load())
        {
            std::vector<uint8_t> chunk;
            {
                std::lock_guard<std::mutex> lock(client->queueMutex);
                if (! client->outgoing.empty())
                {
                    chunk = std::move(client->outgoing.front());
                    client->outgoing.pop_front();
                }
            }
            if (chunk.empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            const auto header = juce::String::toHexString((int) chunk.size()) + "\r\n";
            const auto headerUtf8 = header.toUTF8();
            bool ok = client->socket->write(headerUtf8.getAddress(), (int) strlen(headerUtf8.getAddress())) >= 0;
            ok = ok && client->socket->write(chunk.data(), (int) chunk.size()) >= 0;
            ok = ok && client->socket->write("\r\n", 2) >= 0;
            if (! ok)
                break;
        }

        // Marks itself dead and returns -- does not touch `clients` or join itself. The only
        // thing that ever removes a Client from `clients` (joining its thread first) is
        // CastingStreamServer::stop(), called from the owning CastingStreamServer's thread, not
        // from here. A disconnected-but-not-yet-stopped client sits here as a harmless, bounded
        // (at most a handful, for one short-lived cast session) dead entry that pushBytes()
        // skips until the session actually ends.
        client->alive = false;
    }

    void pushBytes(const void* data, size_t numBytes)
    {
        if (numBytes == 0)
            return;
        std::vector<uint8_t> chunk((const uint8_t*) data, (const uint8_t*) data + numBytes);

        {
            std::lock_guard<std::mutex> lock(historyMutex);
            if (initSegment.size() < initSegmentCacheCap)
                initSegment.insert(initSegment.end(), chunk.begin(), chunk.end());
            else
            {
                liveHistory.push_back(chunk);
                liveHistoryBytes += chunk.size();
                while (liveHistoryBytes > liveHistoryCapBytes && ! liveHistory.empty())
                {
                    liveHistoryBytes -= liveHistory.front().size();
                    liveHistory.pop_front();
                }
            }
        }

        std::vector<std::shared_ptr<Client>> snapshot;
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            snapshot = clients;
        }
        for (auto& client : snapshot)
        {
            if (! client->alive.load())
                continue;
            std::lock_guard<std::mutex> lock(client->queueMutex);
            if ((int) client->outgoing.size() >= outgoingQueueCapacity)
            {
                client->outgoing.pop_front(); // Drop oldest rather than let a slow client grow memory.
                ++client->droppedChunks;
            }
            client->outgoing.push_back(chunk);
        }
    }

    void stop()
    {
        stopRequested = true;
        if (listener)
            listener->close();
        if (acceptThread.joinable())
            acceptThread.join();
        listener = nullptr;

        std::vector<std::shared_ptr<Client>> snapshot;
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            snapshot = clients;
            clients.clear();
        }
        for (auto& client : snapshot)
        {
            client->alive = false;
            if (client->socket)
                client->socket->close();
            if (client->writer.joinable())
                client->writer.join();
        }
        snapshot.clear(); // Only now (after every writer thread has actually exited) do the last
                           // shared_ptr references drop, so a Client never outlives its thread's join.

        {
            std::lock_guard<std::mutex> lock(historyMutex);
            initSegment.clear();
            liveHistory.clear();
            liveHistoryBytes = 0;
        }
        running = false;
    }
};

CastingStreamServer::CastingStreamServer() : impl(std::make_unique<Impl>()) {}
CastingStreamServer::~CastingStreamServer() { stop(); }

juce::String CastingStreamServer::start() { return impl->start(); }
void CastingStreamServer::stop() { impl->stop(); }
void CastingStreamServer::pushBytes(const void* data, size_t numBytes) { impl->pushBytes(data, numBytes); }
bool CastingStreamServer::isRunning() const noexcept { return impl->running.load(); }

} // namespace cs::casting
