#include "TutorialScriptCompiler.h"

namespace cw::tutorial
{
namespace
{
juce::String trimComment(const juce::String& line)
{
    auto hash = line.indexOfChar('#');
    return hash >= 0 ? line.substring(0, hash).trim() : line.trim();
}
}

juce::StringArray ScriptCompiler::tokenizeLine(const juce::String& line)
{
    juce::StringArray tokens;
    juce::String current;
    bool inQuotes = false;

    for (int index = 0; index < line.length(); ++index)
    {
        auto character = line[index];

        if (character == '"')
        {
            current << character;
            inQuotes = ! inQuotes;
            continue;
        }

        if (! inQuotes && juce::CharacterFunctions::isWhitespace(character))
        {
            if (current.isNotEmpty())
            {
                tokens.add(current);
                current.clear();
            }
            continue;
        }

        current << character;
    }

    if (current.isNotEmpty())
        tokens.add(current);

    return tokens;
}

juce::String ScriptCompiler::stripQuotes(const juce::String& text)
{
    auto trimmed = text.trim();
    if (trimmed.length() >= 2 && trimmed.startsWithChar('"') && trimmed.endsWithChar('"'))
        return trimmed.substring(1, trimmed.length() - 1);

    return trimmed;
}

bool ScriptCompiler::parseBool(const juce::String& text, bool& value)
{
    auto normalized = text.trim().toLowerCase();
    if (normalized == "true" || normalized == "yes" || normalized == "on")
    {
        value = true;
        return true;
    }

    if (normalized == "false" || normalized == "no" || normalized == "off")
    {
        value = false;
        return true;
    }

    return false;
}

juce::String ScriptCompiler::makeDefaultId(const juce::String& text)
{
    auto id = text.trim().toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    id = id.replace(" ", "-");
    while (id.contains("--"))
        id = id.replace("--", "-");
    id = id.trimCharactersAtStart("-");
    id = id.trimCharactersAtEnd("-");
    return id.isNotEmpty() ? id : "tutorial";
}

bool ScriptCompiler::parseActionType(const juce::String& text, ActionType& type)
{
    auto normalized = text.trim().toLowerCase();
    if (normalized == "switchworkspace" || normalized == "switch-workspace")
    {
        type = ActionType::switchWorkspace;
        return true;
    }

    if (normalized == "applysignaltemplate" || normalized == "apply-signal-template")
    {
        type = ActionType::applySignalTemplate;
        return true;
    }

    if (normalized == "applygraphmacro" || normalized == "apply-graph-macro")
    {
        type = ActionType::applyGraphMacro;
        return true;
    }

    return false;
}

bool ScriptCompiler::compile(const juce::String& source, Script& script, juce::String& errorMessage) const
{
    script = {};
    Scene currentScene;
    bool hasActiveScene = false;

    auto lines = juce::StringArray::fromLines(source);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        auto line = trimComment(lines[lineIndex]);
        if (line.isEmpty())
            continue;

        auto tokens = tokenizeLine(line);
        if (tokens.isEmpty())
            continue;

        auto command = tokens[0].toLowerCase();
        auto fail = [&](const juce::String& message)
        {
            errorMessage = "Tutorial script line " + juce::String(lineIndex + 1) + ": " + message;
            return false;
        };

        if (command == "tutorial")
        {
            if (tokens.size() < 3)
                return fail("tutorial requires an id and a quoted name.");

            script.id = tokens[1];
            script.name = stripQuotes(line.fromFirstOccurrenceOf(tokens[1], false, false).trim());
            script.name = stripQuotes(script.name);
            if (script.id.isEmpty())
                script.id = makeDefaultId(script.name);
            continue;
        }

        if (command == "description")
        {
            script.description = stripQuotes(line.fromFirstOccurrenceOf(tokens[0], false, false).trim());
            continue;
        }

        if (command == "scene")
        {
            if (tokens.size() < 2)
                return fail("scene requires an id.");

            if (hasActiveScene)
                script.scenes.add(currentScene);

            currentScene = {};
            currentScene.id = tokens[1];
            currentScene.advanceOnTargetClick = true;
            currentScene.drawConnector = true;
            hasActiveScene = true;
            continue;
        }

        if (! hasActiveScene)
            return fail("commands after tutorial header must appear inside a scene.");

        if (command == "title")
        {
            currentScene.title = stripQuotes(line.fromFirstOccurrenceOf(tokens[0], false, false).trim());
            continue;
        }

        if (command == "say")
        {
            auto bodyLine = stripQuotes(line.fromFirstOccurrenceOf(tokens[0], false, false).trim());
            if (currentScene.body.isNotEmpty())
                currentScene.body << "\n\n";
            currentScene.body << bodyLine;
            continue;
        }

        if (command == "focus")
        {
            if (tokens.size() < 2)
                return fail("focus requires a target id.");
            currentScene.targetId = tokens[1];
            continue;
        }

        if (command == "advanceonclick")
        {
            if (tokens.size() < 2)
                return fail("advanceOnClick requires true or false.");

            bool value = true;
            if (! parseBool(tokens[1], value))
                return fail("advanceOnClick expects true or false.");

            currentScene.advanceOnTargetClick = value;
            continue;
        }

        if (command == "connector")
        {
            if (tokens.size() < 2)
                return fail("connector requires true or false.");

            bool value = true;
            if (! parseBool(tokens[1], value))
                return fail("connector expects true or false.");

            currentScene.drawConnector = value;
            continue;
        }

        if (command == "next")
        {
            currentScene.nextButtonText = stripQuotes(line.fromFirstOccurrenceOf(tokens[0], false, false).trim());
            continue;
        }

        if (command == "do")
        {
            if (tokens.size() < 3)
                return fail("do requires an action name and value.");

            ActionType actionType;
            if (! parseActionType(tokens[1], actionType))
                return fail("unknown action '" + tokens[1] + "'.");

            auto actionStart = line.indexOf(tokens[1]);
            if (actionStart < 0)
                return fail("could not locate action payload.");

            auto value = line.substring(actionStart + tokens[1].length()).trim();
            value = stripQuotes(value);

            Action action;
            action.type = actionType;
            action.value = value;
            currentScene.actions.add(action);
            continue;
        }

        if (command == "endscene")
        {
            if (currentScene.id.isEmpty())
                return fail("scene is missing an id.");
            if (currentScene.title.isEmpty())
                currentScene.title = currentScene.id;

            script.scenes.add(currentScene);
            currentScene = {};
            hasActiveScene = false;
            continue;
        }

        return fail("unknown command '" + tokens[0] + "'.");
    }

    if (hasActiveScene)
        script.scenes.add(currentScene);

    if (script.id.isEmpty())
        script.id = makeDefaultId(script.name);

    if (script.name.isEmpty())
        return false ? true : (errorMessage = "Tutorial script is missing a tutorial declaration.", false);

    if (script.scenes.isEmpty())
        return false ? true : (errorMessage = "Tutorial script does not define any scenes.", false);

    for (const auto& scene : script.scenes)
    {
        if (scene.title.isEmpty() || scene.body.isEmpty())
        {
            errorMessage = "Every tutorial scene needs a title and at least one say line.";
            return false;
        }
    }

    return true;
}
}
