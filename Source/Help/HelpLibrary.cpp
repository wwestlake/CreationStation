#include "HelpLibrary.h"

#include "HelpData.h"

#include <algorithm>

namespace cs::help
{
namespace
{
juce::StringArray stringsOf(const juce::var& v)
{
    juce::StringArray out;
    if (auto* a = v.getArray())
        for (const auto& item : *a)
            out.add(item.toString());
    return out;
}

juce::StringArray tokensOf(const juce::String& text)
{
    juce::StringArray out;
    juce::String word;
    auto flush = [&]
    {
        if (word.length() >= 2)
            out.addIfNotAlreadyThere(word);
        word.clear();
    };
    for (auto c : text.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c))
            word << c;
        else
            flush();
    }
    flush();

    // Words that carry no meaning for matching.
    static const juce::StringArray stop { "the", "and", "how", "do", "does", "is", "are", "to", "of", "in", "on", "it", "my", "can",
                                          "what", "why", "for", "with", "this", "that", "me", "you", "use", "an", "or", "be", "at" };
    for (int i = out.size(); --i >= 0;)
        if (stop.contains(out[i]))
            out.remove(i);
    return out;
}

bool wordMatches(const juce::String& haystackLower, const juce::String& word)
{
    return haystackLower.contains(word);
}

juce::String topicText(const Topic& t)
{
    juce::String s = t.title + " " + t.summary;
    for (const auto& b : t.blocks)
        s << " " << Library::plainText(b);
    return s;
}

juce::String snippetOf(const juce::String& text, int maxChars)
{
    auto s = text.replaceCharacters("\r\n", "  ").trim();
    return s.length() > maxChars ? s.substring(0, maxChars).trimEnd() + "..." : s;
}
}

bool Library::addTopicJson(const juce::String& json, juce::String& errorMessage)
{
    juce::var root;
    if (juce::JSON::parse(json, root).failed() || ! root.isObject())
    {
        errorMessage = "not valid JSON";
        return false;
    }

    Topic t;
    t.id = root["id"].toString();
    t.type = root["type"].toString();
    t.title = root["title"].toString();
    t.summary = root["summary"].toString();
    t.status = root["status"].toString();
    t.keywords = stringsOf(root["keywords"]);
    t.alternateQueries = stringsOf(root["alternateQueries"]);
    t.contentRevision = (int) root["verification"]["contentRevision"];

    if (t.id.isEmpty() || t.title.isEmpty())
    {
        errorMessage = "topic has no id or title";
        return false;
    }
    if (findTopic(t.id) != nullptr)
    {
        errorMessage = "duplicate topic id " + t.id;
        return false;
    }

    if (auto* a = root["contexts"].getArray())
        for (const auto& c : *a)
            t.contexts.push_back({ c["helpId"].toString(), c["anchorBlockId"].toString() });
    if (auto* a = root["relations"].getArray())
        for (const auto& r : *a)
            t.relations.push_back({ r["type"].toString(), r["topicId"].toString() });

    if (auto* a = root["blocks"].getArray())
    {
        for (const auto& bv : *a)
        {
            Block b;
            b.id = bv["id"].toString();
            b.kind = bv["kind"].toString();
            b.title = bv["title"].toString();
            b.text = bv["text"].toString();
            b.term = bv["term"].toString();
            b.definition = bv["definition"].toString();
            b.cause = bv["cause"].toString();
            b.resolution = bv["resolution"].toString();
            b.symptoms = stringsOf(bv["symptoms"]);
            if (auto* steps = bv["steps"].getArray())
                for (const auto& s : *steps)
                    b.steps.push_back({ s["id"].toString(), s["instruction"].toString(),
                                        s["expectedResult"].toString(), s["uiTarget"].toString() });
            t.blocks.push_back(std::move(b));
        }
    }

    topicList.push_back(std::move(t));
    return true;
}

bool Library::addInventoryJson(const juce::String& json, juce::String& errorMessage)
{
    juce::var root;
    if (juce::JSON::parse(json, root).failed() || ! root.isObject())
    {
        errorMessage = "inventory is not valid JSON";
        return false;
    }
    if (auto* items = root["items"].getArray())
        for (const auto& item : *items)
            inventoryIds.addIfNotAlreadyThere(item["helpId"].toString());
    return true;
}

bool Library::loadEmbedded(juce::String& errorMessage)
{
    for (int i = 0; i < HelpData::namedResourceListSize; ++i)
    {
        const char* name = HelpData::namedResourceList[i];
        int size = 0;
        const char* data = HelpData::getNamedResource(name, size);
        if (data == nullptr)
            continue;
        const auto original = juce::String(HelpData::getNamedResourceOriginalFilename(name));
        const auto text = juce::String::fromUTF8(data, size);
        juce::String err;

        if (original == "help-inventory.json")
        {
            if (! addInventoryJson(text, err))
            {
                errorMessage = err;
                return false;
            }
        }
        else if (original.startsWith("djehuti.station.") && original.endsWith(".json"))
        {
            if (! addTopicJson(text, err))
            {
                errorMessage = original + ": " + err;
                return false;
            }
        }
    }

    std::sort(topicList.begin(), topicList.end(), [](const Topic& a, const Topic& b) { return a.title.compareIgnoreCase(b.title) < 0; });
    return ! topicList.empty();
}

const Topic* Library::findTopic(const juce::String& topicId) const
{
    for (const auto& t : topicList)
        if (t.id == topicId)
            return &t;
    return nullptr;
}

std::vector<const Topic*> Library::topicsFor(const juce::String& helpId) const
{
    std::vector<const Topic*> out;
    for (const auto& t : topicList)
        for (const auto& c : t.contexts)
            if (c.helpId == helpId)
            {
                out.push_back(&t);
                break;
            }
    return out;
}

const Topic* Library::primaryTopicFor(const juce::String& helpId) const
{
    // The most specific topic wins: one that lists the feature first and covers the fewest other features (so a topic about
    // the Tracker beats the reference that lists every panel), then any that lists it first, then any that lists it at all.
    const Topic* best = nullptr;
    int bestRank = 0;
    for (const auto& t : topicList)
    {
        for (size_t i = 0; i < t.contexts.size(); ++i)
        {
            if (t.contexts[i].helpId != helpId)
                continue;
            const int rank = (i == 0 ? 0 : 100000) + (int) t.contexts.size();
            if (best == nullptr || rank < bestRank)
            {
                best = &t;
                bestRank = rank;
            }
            break;
        }
    }
    return best;
}

juce::String Library::plainText(const Block& b)
{
    if (b.kind == "steps")
    {
        juce::String s = b.title + ":";
        int n = 1;
        for (const auto& st : b.steps)
        {
            s << "\n" << n++ << ". " << st.instruction;
            if (st.expectedResult.isNotEmpty())
                s << " (" << st.expectedResult << ")";
        }
        return s;
    }
    if (b.kind == "definition")
        return b.term + ": " + b.definition;
    if (b.kind == "troubleshooting")
    {
        juce::String s = b.title;
        if (! b.symptoms.isEmpty())
            s << " [" << b.symptoms.joinIntoString("; ") << "]";
        s << ": " << b.resolution;
        if (b.cause.isNotEmpty())
            s << " (Why: " << b.cause << ")";
        return s;
    }
    if (b.kind == "note")
        return "Note: " + b.text;
    if (b.kind == "warning")
        return "Warning: " + b.text;
    return b.text;
}

std::vector<Hit> Library::search(const juce::String& query, int maxResults) const
{
    std::vector<Hit> hits;
    const auto trimmed = query.trim();
    if (trimmed.isEmpty())
        return hits;

    // Exact topic or help ID.
    if (const auto* exact = findTopic(trimmed))
    {
        hits.push_back({ exact, {}, 1000.0, snippetOf(exact->summary, 200) });
    }
    else
    {
        for (const auto* forId : topicsFor(trimmed))
            hits.push_back({ forId, {}, 900.0, snippetOf(forId->summary, 200) });
    }

    const auto words = tokensOf(trimmed);
    const auto phrase = trimmed.toLowerCase();

    for (const auto& t : topicList)
    {
        if (std::any_of(hits.begin(), hits.end(), [&](const Hit& h) { return h.topic == &t; }))
            continue;

        const auto title = t.title.toLowerCase();
        const auto summary = t.summary.toLowerCase();
        const auto keywords = t.keywords.joinIntoString(" | ").toLowerCase();
        const auto questions = t.alternateQueries.joinIntoString(" | ").toLowerCase();

        double score = 0.0;
        if (title.contains(phrase))
            score += 30;
        if (keywords.contains(phrase))
            score += 25;
        if (questions.contains(phrase))
            score += 25;

        for (const auto& w : words)
        {
            if (wordMatches(title, w))
                score += 8;
            if (wordMatches(keywords, w))
                score += 6;
            if (wordMatches(questions, w))
                score += 5;
            if (wordMatches(summary, w))
                score += 3;
        }

        // Best block.
        juce::String bestBlock, bestText;
        double bestBlockScore = 0.0;
        for (const auto& b : t.blocks)
        {
            const auto text = plainText(b);
            const auto lower = text.toLowerCase();
            double bs = lower.contains(phrase) ? 6.0 : 0.0;
            for (const auto& w : words)
                if (wordMatches(lower, w))
                    bs += 1.5;
            if (bs > bestBlockScore)
            {
                bestBlockScore = bs;
                bestBlock = b.id;
                bestText = text;
            }
        }
        score += juce::jmin(bestBlockScore, 12.0);

        // Require most of the words to appear somewhere in the topic, so long questions do not match on one stray word.
        if (! words.isEmpty())
        {
            const auto all = (title + " " + summary + " " + keywords + " " + questions + " " + topicText(t).toLowerCase());
            int found = 0;
            for (const auto& w : words)
                if (all.contains(w))
                    ++found;
            if (found * 2 < words.size())
                continue;
        }

        if (score > 0.0)
            hits.push_back({ &t, bestBlock, score, snippetOf(bestText.isNotEmpty() ? bestText : t.summary, 200) });
    }

    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });
    if ((int) hits.size() > maxResults)
        hits.resize((size_t) maxResults);
    return hits;
}

std::vector<const Topic*> Library::topicsForPrompt(const juce::String& question, const juce::String& contextHelpId) const
{
    std::vector<const Topic*> chosen;
    auto addTopic = [&](const Topic* t)
    {
        if (t != nullptr && std::find(chosen.begin(), chosen.end(), t) == chosen.end())
            chosen.push_back(t);
    };

    // What the question is about comes first; the panel the user is in is added after, because it only says where they are,
    // not what they asked (the assistant panel itself is where most questions are typed).
    for (const auto& h : search(question, 3))
        addTopic(h.topic);
    if (contextHelpId.isNotEmpty())
        addTopic(primaryTopicFor(contextHelpId));
    return chosen;
}

juce::String Library::buildPromptContext(const juce::String& question, const juce::String& contextHelpId, int maxChars) const
{
    const auto chosen = topicsForPrompt(question, contextHelpId);

    juce::String out;
    if (! chosen.empty())
    {
        out << "Topics provided: ";
        for (size_t i = 0; i < chosen.size(); ++i)
            out << (i > 0 ? "; " : "") << chosen[i]->title;
        out << "\n\n";
    }
    for (const auto* t : chosen)
    {
        juce::String section;
        section << "## " << t->title << "  [topic: " << t->id << ", " << t->type << ", status: " << t->status << "]\n"
                << t->summary << "\n";
        for (const auto& b : t->blocks)
            section << plainText(b) << "\n";
        section << "\n";

        if (out.length() + section.length() > maxChars)
        {
            if (out.isEmpty())
                out << section.substring(0, maxChars);
            break;
        }
        out << section;
    }
    return out;
}

juce::StringArray Library::validate() const
{
    juce::StringArray problems;
    juce::StringArray seen;

    for (const auto& t : topicList)
    {
        if (seen.contains(t.id))
            problems.add("duplicate topic " + t.id);
        seen.add(t.id);

        juce::StringArray blockIds;
        for (const auto& b : t.blocks)
        {
            if (blockIds.contains(b.id))
                problems.add(t.id + ": duplicate block " + b.id);
            blockIds.add(b.id);
        }

        for (const auto& r : t.relations)
            if (findTopic(r.topicId) == nullptr)
                problems.add(t.id + ": relation to missing topic " + r.topicId);

        for (const auto& c : t.contexts)
        {
            if (c.anchorBlockId.isNotEmpty() && ! blockIds.contains(c.anchorBlockId))
                problems.add(t.id + ": context " + c.helpId + " anchors missing block " + c.anchorBlockId);
            if (hasInventory() && ! inventoryIds.contains(c.helpId))
                problems.add(t.id + ": context " + c.helpId + " is not in the inventory");
        }
    }
    return problems;
}
}
