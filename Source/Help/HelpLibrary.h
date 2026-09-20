#pragma once

#include <JuceHeader.h>

#include <vector>

namespace cs::help
{
// Station's help, loaded from the structured JSON topics in help/ (see help/README.md). One library serves both readers:
// the help window (people who do not use the assistant) and the Virtual Engineer (which is handed the most relevant
// excerpts with the topic each came from). It is read-only data: nothing here writes to the disk.
struct Step
{
    juce::String id, instruction, expectedResult, uiTarget;
};

struct Block
{
    juce::String id, kind, title, text, intro, term, definition, cause, resolution;
    juce::StringArray symptoms;
    std::vector<Step> steps;
};

struct Context
{
    juce::String helpId, anchorBlockId;
};

struct Relation
{
    juce::String type, topicId;
};

struct Topic
{
    juce::String id, type, title, summary, status;
    juce::StringArray keywords, alternateQueries;
    std::vector<Context> contexts;
    std::vector<Relation> relations;
    std::vector<Block> blocks;
    int contentRevision = 1;
};

struct Hit
{
    const Topic* topic = nullptr;
    juce::String blockId;
    double score = 0.0;
    juce::String snippet;
};

class Library
{
public:
    // Adds one topic from its JSON text. False (with the reason) if it is not a valid topic.
    bool addTopicJson(const juce::String& json, juce::String& errorMessage);
    // Adds the feature inventory (the set of help IDs the application defines), used to check the topics against.
    bool addInventoryJson(const juce::String& json, juce::String& errorMessage);
    // Loads every topic and the inventory that were embedded into the application. See HelpEmbedded.cpp.
    bool loadEmbedded(juce::String& errorMessage);

    const std::vector<Topic>& topics() const noexcept { return topicList; }
    const Topic* findTopic(const juce::String& topicId) const;

    // The topic that best explains one application feature (a help ID such as "djehuti.station.view.tracker"): the topic
    // that lists it first, otherwise the first that lists it at all. Null if no topic covers it.
    const Topic* primaryTopicFor(const juce::String& helpId) const;
    std::vector<const Topic*> topicsFor(const juce::String& helpId) const;

    // Searches titles, keywords, alternative questions, summaries and block text. Exact topic or help IDs come first.
    std::vector<Hit> search(const juce::String& query, int maxResults) const;

    // A block as plain text (steps numbered, definitions as "term: meaning", and so on).
    static juce::String plainText(const Block& block);

    // The topics buildPromptContext draws on, in order: matches for the question first, then the panel's own topic.
    std::vector<const Topic*> topicsForPrompt(const juce::String& question, const juce::String& contextHelpId) const;

    // Excerpts for the Virtual Engineer: the help for the panel the user is in (contextHelpId, may be empty) followed by the
    // best matches for the question, each labelled with its topic so the answer can name its source. At most maxChars.
    juce::String buildPromptContext(const juce::String& question, const juce::String& contextHelpId, int maxChars) const;

    // Everything wrong with the loaded help: duplicate ids, broken relations, anchors that do not exist, help IDs that are
    // not in the inventory. Empty when the help is consistent.
    juce::StringArray validate() const;

    bool hasInventory() const noexcept { return ! inventoryIds.isEmpty(); }

private:
    std::vector<Topic> topicList;
    juce::StringArray inventoryIds;
};
}
