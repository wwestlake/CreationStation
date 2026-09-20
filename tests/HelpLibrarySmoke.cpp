// Checks that the help embedded in Station loads, is internally consistent, and finds the right topic for what
// people (and the Virtual Engineer) ask. Exit code 0 = pass.
#include <JuceHeader.h>

#include "HelpLibrary.h"

namespace
{
int failures = 0;

void check(bool ok, const juce::String& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}

bool topFor(const cs::help::Library& lib, const juce::String& query, const juce::String& topicId, int within = 3)
{
    const auto hits = lib.search(query, within);
    for (const auto& h : hits)
        if (h.topic->id == topicId)
            return true;
    std::cout << "      \"" << query << "\" gave:";
    for (const auto& h : hits)
        std::cout << " " << h.topic->id;
    std::cout << std::endl;
    return false;
}
}

int main()
{
    cs::help::Library lib;
    juce::String err;
    check(lib.loadEmbedded(err), "embedded help loads " + err);
    check(lib.topics().size() >= 47, "at least 47 topics loaded (" + juce::String((int) lib.topics().size()) + ")");
    check(lib.hasInventory(), "inventory loaded");

    const auto problems = lib.validate();
    for (const auto& p : problems)
        std::cout << "      " << p << std::endl;
    check(problems.isEmpty(), "topics are consistent with each other and the inventory");

    // Context help: every view that has a topic resolves to it.
    for (auto* id : { "djehuti.station.view.tracker", "djehuti.station.view.signal-lab", "djehuti.station.view.layers",
                      "djehuti.station.view.video", "djehuti.station.view.virtual-engineer", "djehuti.station.file.import" })
        check(lib.primaryTopicFor(id) != nullptr, juce::String("context help exists for ") + id);
    check(lib.primaryTopicFor("djehuti.station.view.layers") != nullptr
              && lib.primaryTopicFor("djehuti.station.view.layers")->id == "djehuti.station.mixer-layers",
          "Layers panel opens the Layers topic");
    check(lib.primaryTopicFor("no.such.id") == nullptr, "unknown help id has no topic");

    // Search: real questions find the right topic.
    check(topFor(lib, "How do I make a sound?", "djehuti.station.signal-lab-first-sound"), "search: make a sound");
    check(topFor(lib, "why is my video black", "djehuti.station.troubleshoot-video"), "search: black video");
    check(topFor(lib, "import a big video", "djehuti.station.assets-import"), "search: import video");
    check(topFor(lib, "X-Touch", "djehuti.station.control-surfaces", 1), "search: X-Touch");
    check(topFor(lib, "djehuti.station.overview", "djehuti.station.overview", 1), "search: exact topic id first");
    check(lib.search("zzzqqqxxx nonsense", 5).empty(), "search: nonsense finds nothing");

    // Virtual Engineer context.
    const auto ctx = lib.buildPromptContext("How do I import a video?", "djehuti.station.view.tracker", 6000);
    check(ctx.isNotEmpty() && ctx.length() <= 6000, "prompt context is present and within the limit");
    check(ctx.contains("[topic: djehuti.station.tracker-overview"), "prompt context also includes the panel the user is in");
    check(ctx.contains("[topic: djehuti.station.assets-import"), "prompt context includes the import topic with its id");
    check(lib.buildPromptContext("qqqzzz", {}, 6000).isEmpty(), "no context when nothing matches");
    check(lib.buildPromptContext("How do I import a video?", {}, 500).length() <= 500, "prompt context respects a small limit");

    // The exact question typed into the assistant while it has focus.
    const auto vctx = lib.buildPromptContext("how do I  import a video?", "djehuti.station.view.virtual-engineer", 7000);
    check(vctx.contains("[topic: djehuti.station.assets-import") || vctx.contains("[topic: djehuti.station.video-import-play"),
          "assistant question about importing a video reaches an import topic");
    check(vctx.indexOf("[topic: djehuti.station.virtual-engineer") > vctx.indexOf("import"),
          "the assistant panel's own topic does not crowd out what was asked");

    // Bad input is refused.
    cs::help::Library bad;
    check(! bad.addTopicJson("{ not json", err), "invalid JSON rejected");
    check(! bad.addTopicJson("{\"type\":\"how-to\"}", err), "topic without id rejected");

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + juce::String(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
