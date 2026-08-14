#include "VstPluginCatalog.h"

namespace
{
juce::AudioPluginFormatManager& getScanFormatManager()
{
    static juce::AudioPluginFormatManager manager;
    static bool initialised = false;
    if (! initialised)
    {
        manager.addDefaultFormats();
        initialised = true;
    }
    return manager;
}
}

void VstPluginCatalog::setSearchPaths(const juce::StringArray& newPaths)
{
    searchPaths = newPaths;
    searchPaths.trim();
    searchPaths.removeEmptyStrings();
    searchPaths.removeDuplicates(false);
}

void VstPluginCatalog::rescan()
{
    entries.clear();

    auto& formatManager = getScanFormatManager();

    // Identifies the plugin(s) a file actually contains via its real VST3 metadata (name,
    // manufacturer, category, and critically whether it's an instrument or an effect) rather than
    // guessing from the filename - a single .vst3 can also describe more than one plugin.
    auto addEntriesForFile = [&](const juce::File& file, const juce::String& displayName)
    {
        juce::OwnedArray<juce::PluginDescription> descriptions;
        for (auto* format : formatManager.getFormats())
            if (format != nullptr && format->fileMightContainThisPluginType(file.getFullPathName()))
                format->findAllTypesForFile(descriptions, file.getFullPathName());

        if (descriptions.isEmpty())
        {
            // Could not identify the plugin type - still list it (with a best-effort name-based
            // guess) rather than silently dropping it from the catalog.
            Entry entry { displayName, file };
            parsePluginMetadataFallback(entry);
            entries.add(entry);
            return;
        }

        for (auto* description : descriptions)
        {
            Entry entry { displayName, file };
            parsePluginMetadata(entry, *description);
            entries.add(entry);
        }
    };

    // User-configured search paths are scanned first and always kept: these are real,
    // intentionally-installed plugin locations (e.g. a shared folder also used by another host).
    juce::StringArray seenPaths;
    juce::StringArray seenNames;
    for (const auto& path : searchPaths)
    {
        auto directory = juce::File(path.trim());
        if (! directory.isDirectory())
            continue;

        juce::Array<juce::File> foundFiles;
        directory.findChildFiles(foundFiles, juce::File::findFilesAndDirectories, true, "*.vst3");

        for (const auto& file : foundFiles)
        {
            auto fullPath = file.getFullPathName();
            if (seenPaths.contains(fullPath))
                continue;

            // Skip the binary inside a VST3 bundle (e.g. Foo.vst3\Contents\x86_64-win\Foo.vst3):
            // a recursive *.vst3 scan matches both the outer bundle folder and this inner file, so
            // only list the outer bundle. Any *.vst3 whose parent path already contains ".vst3" is
            // nested inside a bundle.
            if (file.getParentDirectory().getFullPathName().containsIgnoreCase(".vst3"))
                continue;

            seenPaths.add(fullPath);
            auto displayName = makeDisplayName(file);
            seenNames.add(displayName);
            addEntriesForFile(file, displayName);
        }
    }

    // The dev build folder is scanned last and only contributes plugins that aren't already
    // present under a user-configured path: the same Creation Station plugin often ends up
    // copied into a real plugin folder (e.g. to use it in another host), which is the same
    // binary as the freshly-built one here, not a distinct 32-bit/64-bit variant.
    auto appExe = juce::File::getSpecialLocation(juce::File::currentApplicationFile);

    // Always scan a "Plugins" folder sitting directly next to the running executable, no
    // path configuration required - this is where every first-party plugin's build (see
    // creation_station_copy_plugin_to_scanned_folder in cmake/CreationStationPlugin.cmake)
    // gets copied, both in the raw build tree and in the shared claude-<config>-bin copy, so
    // it works identically regardless of which copy of the exe is actually running (unlike
    // the "_artefacts" climb below, which only resolves from the raw build tree).
    auto builtInPluginsDirectory = appExe.getParentDirectory().getChildFile("Plugins");
    if (builtInPluginsDirectory.isDirectory())
    {
        juce::Array<juce::File> foundFiles;
        builtInPluginsDirectory.findChildFiles(foundFiles, juce::File::findFilesAndDirectories, true, "*.vst3");

        for (const auto& file : foundFiles)
        {
            auto fullPath = file.getFullPathName();
            if (seenPaths.contains(fullPath))
                continue;

            if (file.getParentDirectory().getFullPathName().containsIgnoreCase(".vst3"))
                continue;

            seenPaths.add(fullPath);
            auto displayName = makeDisplayName(file);
            seenNames.add(displayName);
            addEntriesForFile(file, displayName);
        }
    }

    // Path is like: D:/000 Creation Station/build/CreativeWorkstation_artefacts/Release/Creative Workstation.exe
    // We want: D:/000 Creation Station/build/
    // Only trust this climb when the exe actually sits in that layout (checked via the
    // "_artefacts" folder name one level up) - when running from the shared
    // claude-<config>-bin copy (see AGENTS.md) the same three-hop climb lands on a drive
    // root instead, and a recursive *.vst3 scan from there walks the entire drive.
    auto artefactsFolder = appExe.getParentDirectory().getParentDirectory();
    auto buildFolder = artefactsFolder.getParentDirectory();
    if (artefactsFolder.getFileName().endsWithIgnoreCase("_artefacts") && buildFolder.isDirectory())
    {
        juce::Array<juce::File> foundFiles;
        buildFolder.findChildFiles(foundFiles, juce::File::findFilesAndDirectories, true, "*.vst3");

        for (const auto& file : foundFiles)
        {
            auto fullPath = file.getFullPathName();
            if (seenPaths.contains(fullPath))
                continue;

            if (file.getParentDirectory().getFullPathName().containsIgnoreCase(".vst3"))
                continue;

            auto displayName = makeDisplayName(file);
            if (seenNames.contains(displayName, true))
                continue;

            seenPaths.add(fullPath);
            seenNames.add(displayName);
            addEntriesForFile(file, displayName);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
    {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
}

juce::String VstPluginCatalog::describeSummary() const
{
    if (entries.isEmpty())
        return searchPaths.isEmpty() ? "No VST folders configured." : "No VST plugins found in configured folders.";

    return juce::String(entries.size()) + " VST plugin(s) indexed.";
}

juce::String VstPluginCatalog::makeDisplayName(const juce::File& file)
{
    auto name = file.getFileNameWithoutExtension();
    if (name.isEmpty())
        name = file.getFileName();

    return name;
}

void VstPluginCatalog::parsePluginMetadata(Entry& entry, const juce::PluginDescription& description)
{
    entry.format = PluginFormat::VST3;
    entry.manufacturer = description.manufacturerName.isNotEmpty() ? description.manufacturerName : "Unknown";

    // Our own plugins (COMPANY_NAME "LagDaemon Software" in CreationStationPlugin.cmake) aren't
    // special-cased here - they fall through to the same isInstrument/category detection as any
    // third-party plugin, so each one lands in its real category ("Effects: EQ", "Instruments",
    // etc.) rather than being lumped into one bucket. The browser's dedicated "Creation Station
    // Plugins" section (built in PluginBrowserList) is a second, independent view keyed off this
    // same manufacturer field - plugins appear in both places, same as any developer/category
    // section is just a filtered view, not an exclusive container.

    // isInstrument comes straight from the plugin's own declared VST3 category (kVstCategory
    // Instrument, or the classic "isSynth" flag for older formats) - this is what actually
    // distinguishes a sound-producing instrument (playable, needs MIDI in, produces audio with no
    // audio input) from an effect/processor (takes audio in, transforms it), not the plugin's name.
    if (description.isInstrument)
    {
        entry.type = PluginType::Instrument;

        // VST3's own category string often already has useful detail (e.g. "Instrument|Synth",
        // "Instrument|Drum", "Instrument|Sampler") - fall back to a generic "Synth" bucket only
        // when it doesn't.
        auto category = description.category;
        if (category.containsIgnoreCase("drum"))
            entry.category = "Drum";
        else if (category.containsIgnoreCase("sampler"))
            entry.category = "Sampler";
        else
            entry.category = "Synth";

        return;
    }

    entry.type = PluginType::Effect;

    // VST3 effect categories look like "Fx|Reverb", "Fx|Dynamics", "Fx|EQ" etc. - use that
    // directly when present, since it's the plugin's own declared category, not a guess.
    auto category = description.category;
    if (category.containsIgnoreCase("reverb"))
        entry.category = "Reverb";
    else if (category.containsIgnoreCase("delay"))
        entry.category = "Delay";
    else if (category.containsIgnoreCase("dynamics"))
        entry.category = "Dynamics";
    else if (category.containsIgnoreCase("eq"))
        entry.category = "EQ";
    else if (category.containsIgnoreCase("distortion") || category.containsIgnoreCase("distort"))
        entry.category = "Distortion";
    else if (category.containsIgnoreCase("modulation"))
        entry.category = "Modulation";
    else if (category.containsIgnoreCase("filter"))
        entry.category = "Filter";
    else if (category.containsIgnoreCase("pitch"))
        entry.category = "Pitch Shift";
    else if (category.containsIgnoreCase("spatial") || category.containsIgnoreCase("surround"))
        entry.category = "Spatial";
    else if (category.containsIgnoreCase("mastering"))
        entry.category = "Mastering";
    else
        parsePluginMetadataFallback(entry); // Category was too generic ("Fx") - fall back to a name-based guess.
}

void VstPluginCatalog::parsePluginMetadataFallback(Entry& entry)
{
    // Only used when the plugin's real metadata couldn't be read, or its declared category was
    // too generic to bucket usefully - a best-effort guess from the plugin's name, not a
    // substitute for reading isInstrument (which is why this never sets PluginType::Instrument
    // on its own confidently enough to be trusted - a name containing "synth" is a reasonable
    // hint, but description.isInstrument is what actually governs playability).
    entry.format = PluginFormat::VST3;
    if (entry.manufacturer.isEmpty())
        entry.manufacturer = "Unknown";

    auto nameLower = entry.name.toLowerCase();
    if (nameLower.contains("synth") || nameLower.contains("drum") || nameLower.contains("sampler"))
    {
        entry.type = PluginType::Instrument;
        entry.category = "Synth";
        return;
    }

    entry.type = PluginType::Effect;

    if (nameLower.contains("delay") || nameLower.contains("reverb"))
        entry.category = "Reverb";
    else if (nameLower.contains("eq") || nameLower.contains("equali"))
        entry.category = "EQ";
    else if (nameLower.contains("compressor") || nameLower.contains("limiter") ||
             nameLower.contains("gate") || nameLower.contains("expand"))
        entry.category = "Dynamics";
    else if (nameLower.contains("distort") || nameLower.contains("overdrive") ||
             nameLower.contains("fuzz") || nameLower.contains("saturat"))
        entry.category = "Distortion";
    else if (nameLower.contains("flanger") || nameLower.contains("phaser") ||
             nameLower.contains("chorus") || nameLower.contains("modula"))
        entry.category = "Modulation";
    else if (nameLower.contains("pitch") || nameLower.contains("tuner") ||
             nameLower.contains("transpose"))
        entry.category = "Pitch Shift";
    else if (nameLower.contains("filter"))
        entry.category = "Filter";
    else
        entry.category = "Utility";
}

juce::StringArray VstPluginCatalog::getCategories(PluginType type) const
{
    juce::StringArray categories;
    for (const auto& entry : entries)
    {
        if (entry.type == type && entry.category.isNotEmpty())
        {
            if (! categories.contains(entry.category, true))
                categories.add(entry.category);
        }
    }
    categories.sort(true);
    return categories;
}

juce::Array<VstPluginCatalog::Entry> VstPluginCatalog::getPluginsByCategory(PluginType type, const juce::String& category) const
{
    juce::Array<Entry> result;
    for (const auto& entry : entries)
    {
        if (entry.type == type && entry.category.equalsIgnoreCase(category))
            result.add(entry);
    }
    return result;
}

juce::Array<VstPluginCatalog::Entry> VstPluginCatalog::getPluginsByType(PluginType type) const
{
    juce::Array<Entry> result;
    for (const auto& entry : entries)
    {
        if (entry.type == type)
            result.add(entry);
    }
    return result;
}

juce::StringArray VstPluginCatalog::getManufacturers() const
{
    juce::StringArray manufacturers;
    for (const auto& entry : entries)
    {
        if (entry.manufacturer.isNotEmpty() && ! manufacturers.contains(entry.manufacturer, true))
            manufacturers.add(entry.manufacturer);
    }
    manufacturers.sort(true);
    return manufacturers;
}

juce::Array<VstPluginCatalog::Entry> VstPluginCatalog::getPluginsByManufacturer(const juce::String& manufacturer) const
{
    juce::Array<Entry> result;
    for (const auto& entry : entries)
    {
        if (entry.manufacturer.equalsIgnoreCase(manufacturer))
            result.add(entry);
    }
    return result;
}
