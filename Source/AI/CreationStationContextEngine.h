#pragma once

#include <JuceHeader.h>

class CreationStationContextEngine final : private juce::Thread
{
public:
    struct SourceDocument
    {
        juce::String id;
        juce::String title;
        juce::String category;
        juce::String body;
        juce::String sourcePath;
        juce::StringArray tags;
        juce::Time updatedAt;
    };

    struct RetrievalRequest
    {
        juce::String prompt;
        juce::String workspaceMode;
        juce::String projectName;
        int maxItems = 6;
    };

    struct DynamicsState
    {
        float semanticVelocity = 0.0f;
        float referenceDrift = 0.0f;
        float curvature = 0.0f;
        float torsionalResistance = 0.0f;
        bool recoverySuggested = false;
    };

    struct ContextSnippet
    {
        juce::String documentId;
        juce::String title;
        juce::String category;
        juce::String excerpt;
        float relevanceScore = 0.0f;
    };

    struct ContextPacket
    {
        RetrievalRequest request;
        DynamicsState dynamics;
        juce::String summary;
        juce::Array<ContextSnippet> snippets;
    };

    CreationStationContextEngine();
    ~CreationStationContextEngine() override;

    void upsertDocument(const SourceDocument& document);
    void clearDocuments();
    void submitRequest(const RetrievalRequest& request);
    ContextPacket getLastPacket() const;

    std::function<void(const ContextPacket&)> onContextReady;

private:
    void run() override;

    static juce::StringArray tokenize(const juce::String& text);
    ContextPacket buildPacket(const RetrievalRequest& request);
    DynamicsState computeDynamics(const juce::StringArray& promptTokens);
    juce::String buildSummary(const ContextPacket& packet) const;
    static juce::String makeExcerpt(const juce::String& sourceText, const juce::StringArray& tokens);

    mutable juce::CriticalSection lock;
    juce::WaitableEvent wakeEvent;
    juce::Array<SourceDocument> documents;
    RetrievalRequest pendingRequest;
    bool hasPendingRequest = false;
    ContextPacket lastPacket;
    juce::StringArray anchorTokens;
    juce::StringArray previousTokens;
    float previousVelocity = 0.0f;
};
