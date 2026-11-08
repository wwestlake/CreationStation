#pragma once

#include <memory>
#include <juce_core/juce_core.h>

namespace cs::casting
{

// A minimal, local-network-only HTTP server that serves the live fMP4 byte stream
// CastingMp4Sink produces to exactly the one receiver a cast session is for. Not a general
// web server: it serves one fixed content stream at one random, per-session URL path, and
// nothing else, deliberately kept small rather than reused from a general-purpose HTTP library
// Station doesn't otherwise depend on.
//
// Binds to the PC's own LAN-facing interface address (never 0.0.0.0/every interface, and never
// loopback, which a real TV on the network couldn't reach), so the endpoint is reachable only
// from this network -- and only while a cast session is actually active, since start()/stop()
// bracket the session's lifetime exactly.
class CastingStreamServer
{
public:
    CastingStreamServer();
    ~CastingStreamServer();

    // Starts listening on an OS-assigned port on this machine's LAN-facing address (chosen
    // automatically). Returns the full URL a receiver should be given
    // ("http://192.168.x.y:PORT/cast/<random-token>"), or an empty string if no suitable local
    // address could be found or the socket couldn't be opened -- callers must treat that as
    // "stream server failed" and not proceed.
    juce::String start();

    // Stops listening, disconnects any connected client, and releases the socket. Safe to call
    // even if start() was never called or already stopped.
    void stop();

    // Appends newly produced stream bytes for delivery to the connected client (or cached for
    // the next one to connect, up to a bounded amount of history -- see the .cpp for the exact
    // bound). Safe to call from any thread; never blocks on network I/O itself.
    void pushBytes(const void* data, size_t numBytes);

    bool isRunning() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE(CastingStreamServer)
};

} // namespace cs::casting
