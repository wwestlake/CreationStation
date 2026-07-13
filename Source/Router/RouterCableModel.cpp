#include "RouterCableModel.h"

RouterCableModel::RouterCableModel()
{
    cables.add({ CableRole::monitor, "DjeRoute Monitor", "Speakers and headphones", true });
    cables.add({ CableRole::capture, "DjeRoute Capture", "OBS and recording apps", true });
    cables.add({ CableRole::mic, "DjeRoute Mic", "Microphone workflows", false });
    cables.add({ CableRole::instrument, "DjeRoute Instrument", "Guitar and line input workflows", false });
}

void RouterCableModel::setCableEnabled(int index, bool shouldEnable)
{
    if (juce::isPositiveAndBelow(index, cables.size()))
        cables.getReference(index).enabled = shouldEnable;
}

bool RouterCableModel::isCableEnabled(int index) const
{
    return juce::isPositiveAndBelow(index, cables.size()) ? cables.getReference(index).enabled : false;
}

juce::String RouterCableModel::getSourceName(int index) const
{
    switch (juce::jlimit(0, 4, index))
    {
        case 0: return "Practice Source";
        case 1: return "Mic";
        case 2: return "Instrument";
        case 3: return "System Audio";
        case 4: return "Reaper Monitor";
        default: break;
    }

    return "Source " + juce::String(index + 1);
}

juce::String RouterCableModel::buildSummary() const
{
    juce::StringArray active;
    for (const auto& cable : cables)
        if (cable.enabled)
            active.add(cable.name);

    if (active.isEmpty())
        return "No cables are active yet.";

    return getSourceName(sourceIndex) + " -> " + active.joinIntoString(" + ");
}

juce::StringArray RouterCableModel::getCableNames() const
{
    juce::StringArray names;
    for (const auto& cable : cables)
        names.add(cable.name);
    return names;
}

juce::StringArray RouterCableModel::getCablePurposes() const
{
    juce::StringArray purposes;
    for (const auto& cable : cables)
        purposes.add(cable.purpose);
    return purposes;
}
