#include "PatinaTypes.h"

namespace cw::patina
{
juce::String TypeSpec::toString() const
{
    if (rawText.isNotEmpty())
        return rawText;

    if (genericArguments.isEmpty())
        return family;

    return family + "<" + genericArguments.joinIntoString(", ") + ">";
}

TypeSpec TypeSystem::parseType(const juce::String& text) const
{
    TypeSpec type;
    auto trimmed = text.trim();
    type.rawText = trimmed;

    auto open = trimmed.indexOfChar('<');
    auto close = trimmed.lastIndexOfChar('>');

    if (open > 0 && close > open)
    {
        type.family = trimmed.substring(0, open).trim();
        auto inner = trimmed.substring(open + 1, close);
        type.genericArguments.addTokens(inner, ",", "");
        for (auto& argument : type.genericArguments)
            argument = argument.trim();
    }
    else
    {
        type.family = trimmed;
    }

    return type;
}

bool TypeSystem::isAssignableTo(const TypeSpec& source, const TypeSpec& destination) const
{
    if (! source.isValid() || ! destination.isValid())
        return false;

    if (source.toString() == destination.toString())
        return true;

    if (source.family == "i64" && destination.family == "f32")
        return true;

    if ((source.family == "f32" || source.family == "i64")
        && destination.family == "control"
        && destination.genericArguments.size() == 1
        && destination.genericArguments[0] == "f32")
        return true;

    if (source.family == "bool"
        && destination.family == "control"
        && destination.genericArguments.size() == 1
        && destination.genericArguments[0] == "bool")
        return true;

    return false;
}
} // namespace cw::patina
