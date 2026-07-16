#include "CreationStationContextEngine.h"

namespace
{
float jaccardSimilarity(const juce::StringArray& a, const juce::StringArray& b)
{
    juce::StringArray uniqueA(a);
    juce::StringArray uniqueB(b);
    uniqueA.removeDuplicates(false);
    uniqueB.removeDuplicates(false);

    if (uniqueA.isEmpty() && uniqueB.isEmpty())
        return 1.0f;

    int intersection = 0;
    for (const auto& token : uniqueA)
        if (uniqueB.contains(token))
            ++intersection;

    auto unionCount = uniqueA.size();
    for (const auto& token : uniqueB)
        if (! uniqueA.contains(token))
            ++unionCount;

    return unionCount > 0 ? (float) intersection / (float) unionCount : 0.0f;
}
}

CreationStationContextEngine::CreationStationContextEngine()
    : juce::Thread("CreationStationContextEngine")
{
    startThread();
}

CreationStationContextEngine::~CreationStationContextEngine()
{
    signalThreadShouldExit();
    wakeEvent.signal();
    stopThread(4000);
}

void CreationStationContextEngine::upsertDocument(const SourceDocument& document)
{
    const juce::ScopedLock sl(lock);

    for (auto& existing : documents)
    {
        if (existing.id == document.id)
        {
            existing = document;
            return;
        }
    }

    documents.add(document);
}

void CreationStationContextEngine::replaceDocuments(const juce::Array<SourceDocument>& newDocuments)
{
    const juce::ScopedLock sl(lock);
    documents = newDocuments;
}

void CreationStationContextEngine::clearDocuments()
{
    const juce::ScopedLock sl(lock);
    documents.clear();
}

void CreationStationContextEngine::submitRequest(const RetrievalRequest& request)
{
    {
        const juce::ScopedLock sl(lock);
        pendingRequest = request;
        hasPendingRequest = true;
    }

    wakeEvent.signal();
}

CreationStationContextEngine::ContextPacket CreationStationContextEngine::getLastPacket() const
{
    const juce::ScopedLock sl(lock);
    return lastPacket;
}

void CreationStationContextEngine::run()
{
    while (! threadShouldExit())
    {
        wakeEvent.wait(-1);
        if (threadShouldExit())
            break;

        RetrievalRequest request;
        {
            const juce::ScopedLock sl(lock);
            if (! hasPendingRequest)
                continue;

            request = pendingRequest;
            hasPendingRequest = false;
        }

        auto packet = buildPacket(request);

        {
            const juce::ScopedLock sl(lock);
            lastPacket = packet;
        }

        if (onContextReady)
        {
            juce::MessageManager::callAsync([callback = onContextReady, packet]()
            {
                if (callback)
                    callback(packet);
            });
        }
    }
}

juce::StringArray CreationStationContextEngine::tokenize(const juce::String& text)
{
    auto normalized = text.toLowerCase();
    normalized = normalized.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 _-@./");

    juce::StringArray tokens;
    tokens.addTokens(normalized, " \r\n\t", "");
    tokens.trim();
    tokens.removeEmptyStrings();
    tokens.removeDuplicates(false);
    return tokens;
}

CreationStationContextEngine::ContextPacket CreationStationContextEngine::buildPacket(const RetrievalRequest& request)
{
    ContextPacket packet;
    packet.request = request;

    juce::Array<SourceDocument> snapshot;
    {
        const juce::ScopedLock sl(lock);
        snapshot = documents;
    }

    auto promptTokens = tokenize(request.prompt);
    packet.dynamics = computeDynamics(promptTokens);

    struct RankedItem
    {
        ContextSnippet snippet;
        float score = 0.0f;
    };

    juce::Array<RankedItem> rankedItems;
    for (const auto& document : snapshot)
    {
        auto docTokens = tokenize(document.title + " " + document.category + " " + document.body + " " + document.tags.joinIntoString(" "));
        auto tokenScore = jaccardSimilarity(promptTokens, docTokens);
        auto freshnessHours = juce::jmax(0.0, document.updatedAt.toMilliseconds() > 0
                                                  ? (juce::Time::getCurrentTime() - document.updatedAt).inHours()
                                                  : 72.0);
        auto freshnessBoost = (float) juce::jmap(juce::jlimit(0.0, 72.0, freshnessHours), 72.0, 0.0, 0.0, 0.15);
        auto modeBoost = document.tags.contains(request.workspaceMode.toLowerCase()) ? 0.10f : 0.0f;
        auto score = tokenScore + freshnessBoost + modeBoost;

        if (score <= 0.01f)
            continue;

        RankedItem item;
        item.score = score;
        item.snippet.documentId = document.id;
        item.snippet.title = document.title;
        item.snippet.category = document.category;
        item.snippet.relevanceScore = score;
        item.snippet.excerpt = makeExcerpt(document.body, promptTokens);
        rankedItems.add(item);
    }

    std::sort(rankedItems.begin(), rankedItems.end(), [](const RankedItem& left, const RankedItem& right)
    {
        return left.score > right.score;
    });

    for (int index = 0; index < juce::jmin(request.maxItems, rankedItems.size()); ++index)
        packet.snippets.add(rankedItems.getReference(index).snippet);

    packet.summary = buildSummary(packet);
    return packet;
}

CreationStationContextEngine::DynamicsState CreationStationContextEngine::computeDynamics(const juce::StringArray& promptTokens)
{
    const juce::ScopedLock sl(lock);

    DynamicsState state;

    if (anchorTokens.isEmpty())
        anchorTokens = promptTokens;

    auto continuity = jaccardSimilarity(previousTokens, promptTokens);
    auto anchorSimilarity = jaccardSimilarity(anchorTokens, promptTokens);

    state.semanticVelocity = 1.0f - continuity;
    state.referenceDrift = 1.0f - anchorSimilarity;
    state.curvature = std::abs(state.semanticVelocity - previousVelocity);
    state.torsionalResistance = juce::jlimit(0.0f, 1.0f, (state.referenceDrift * 0.6f) + (state.curvature * 0.4f));
    state.recoverySuggested = state.referenceDrift > 0.72f && state.curvature > 0.45f;

    previousTokens = promptTokens;
    previousVelocity = state.semanticVelocity;
    return state;
}

juce::String CreationStationContextEngine::buildSummary(const ContextPacket& packet) const
{
    juce::String summary;
    summary << "Context packet ready\n";
    summary << "Mode: " << packet.request.workspaceMode
            << "  |  Project: " << (packet.request.projectName.isNotEmpty() ? packet.request.projectName : "none") << "\n";
    summary << "Velocity: " << juce::String(packet.dynamics.semanticVelocity, 2)
            << "  |  Drift: " << juce::String(packet.dynamics.referenceDrift, 2)
            << "  |  Curvature: " << juce::String(packet.dynamics.curvature, 2)
            << "  |  Torsion: " << juce::String(packet.dynamics.torsionalResistance, 2) << "\n";

    if (packet.dynamics.recoverySuggested)
        summary << "Recovery hint: strong thematic pivot detected; broaden retrieval and down-rank stale context.\n";

    summary << "\nTop context:\n";
    for (const auto& snippet : packet.snippets)
        summary << "- [" << snippet.category << "] " << snippet.title << " (" << juce::String(snippet.relevanceScore, 2) << ")\n";

    return summary;
}

juce::String CreationStationContextEngine::makeExcerpt(const juce::String& sourceText, const juce::StringArray& tokens)
{
    auto trimmed = sourceText.trim();
    if (trimmed.length() <= 220)
        return trimmed;

    for (const auto& token : tokens)
    {
        auto index = trimmed.toLowerCase().indexOf(token.toLowerCase());
        if (index >= 0)
        {
            auto start = juce::jmax(0, index - 60);
            return trimmed.substring(start, juce::jmin(trimmed.length(), start + 220)).trim();
        }
    }

    return trimmed.substring(0, 220).trim();
}
