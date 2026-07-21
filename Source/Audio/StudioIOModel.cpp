#include "StudioIOModel.h"

namespace cs
{
juce::String StudioIOModel::makeDefaultName(int channelIndex)
{
    return "Studio Input " + juce::String(channelIndex + 1).paddedLeft('0', 2);
}

void StudioIOModel::ensureInputForHardware(const HardwareInputSource& hardwareInput)
{
    for (auto& input : inputs)
    {
        if (input.channelIndex == hardwareInput.channelIndex)
        {
            input.id = hardwareInput.id.isNotEmpty() ? hardwareInput.id : input.id;
            input.hardwareName = hardwareInput.name;
            input.available = true;

            if (input.name.isEmpty())
                input.name = makeDefaultName(hardwareInput.channelIndex);

            return;
        }
    }

    StudioInputSource input;
    input.id = hardwareInput.id.isNotEmpty() ? hardwareInput.id : ("studio-input-" + juce::String(hardwareInput.channelIndex + 1));
    input.name = makeDefaultName(hardwareInput.channelIndex);
    input.channelIndex = hardwareInput.channelIndex;
    input.hardwareName = hardwareInput.name;
    input.available = true;
    inputs.push_back(std::move(input));
}

void StudioIOModel::setHardwareInputs(const juce::Array<HardwareInputSource>& hardwareInputs)
{
    for (auto& input : inputs)
        input.available = false;

    for (const auto& hardwareInput : hardwareInputs)
    {
        if (hardwareInput.channelIndex < 0)
            continue;

        ensureInputForHardware(hardwareInput);
    }

    std::sort(inputs.begin(), inputs.end(), [](const StudioInputSource& left, const StudioInputSource& right)
    {
        return left.channelIndex < right.channelIndex;
    });
}

juce::Array<juce::String> StudioIOModel::getDisplayNames() const
{
    juce::Array<juce::String> names;

    for (const auto& input : inputs)
    {
        auto label = input.name.isNotEmpty() ? input.name : makeDefaultName(input.channelIndex);

        if (input.hardwareName.isNotEmpty())
            label << "  <-  " << input.hardwareName;

        if (! input.available)
            label << "  (missing)";

        names.add(label);
    }

    return names;
}

juce::StringArray StudioIOModel::getNames() const
{
    juce::StringArray names;

    for (const auto& input : inputs)
        names.add(input.name.isNotEmpty() ? input.name : makeDefaultName(input.channelIndex));

    return names;
}

juce::StringArray StudioIOModel::getHardwareNames() const
{
    juce::StringArray names;

    for (const auto& input : inputs)
        names.add(input.hardwareName);

    return names;
}

juce::Array<bool> StudioIOModel::getAvailability() const
{
    juce::Array<bool> availability;

    for (const auto& input : inputs)
        availability.add(input.available);

    return availability;
}

int StudioIOModel::getChannelForInputIndex(int inputIndex) const noexcept
{
    if (inputIndex >= 0 && inputIndex < static_cast<int>(inputs.size()))
        return inputs[(size_t) inputIndex].available ? inputs[(size_t) inputIndex].channelIndex : -1;

    return -1;
}

int StudioIOModel::getInputIndexForChannel(int channelIndex) const noexcept
{
    for (int index = 0; index < static_cast<int>(inputs.size()); ++index)
        if (inputs[(size_t) index].channelIndex == channelIndex)
            return index;

    return inputs.empty() ? -1 : 0;
}

void StudioIOModel::setInputName(int inputIndex, const juce::String& name)
{
    if (! juce::isPositiveAndBelow(inputIndex, static_cast<int>(inputs.size())))
        return;

    auto trimmed = name.trim();
    inputs[(size_t) inputIndex].name = trimmed.isNotEmpty()
        ? trimmed
        : makeDefaultName(inputs[(size_t) inputIndex].channelIndex);
}

juce::ValueTree StudioIOModel::createState() const
{
    juce::ValueTree state("StudioIO");

    for (const auto& input : inputs)
    {
        juce::ValueTree child("Input");
        child.setProperty("id", input.id, nullptr);
        child.setProperty("name", input.name, nullptr);
        child.setProperty("channelIndex", input.channelIndex, nullptr);
        state.addChild(child, -1, nullptr);
    }

    return state;
}

void StudioIOModel::restoreState(const juce::ValueTree& state)
{
    if (! state.isValid() || state.getType() != juce::Identifier("StudioIO"))
        return;

    inputs.clear();

    for (const auto child : state)
    {
        if (! child.hasType("Input"))
            continue;

        StudioInputSource input;
        input.id = child.getProperty("id").toString();
        input.name = child.getProperty("name").toString();
        input.channelIndex = (int) child.getProperty("channelIndex", -1);
        input.available = false;

        if (input.channelIndex >= 0)
            inputs.push_back(std::move(input));
    }
}
}
