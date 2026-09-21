#pragma once

#include <JuceHeader.h>

#include <creation/assets/ProjectSession.h>
#include <creation/assistant/AssistantFiles.h>

// The assistant's area of the project, backed by the project's own storage (the VFS): conversations, plans and what the
// assistant writes live inside the project, so they travel with it. Message thread only, like the session itself.
class VfsAssistantFiles final : public creation::assistant::AssistantFiles
{
public:
    explicit VfsAssistantFiles(creation::assets::ProjectSession& sessionToUse) : session(sessionToUse) {}

    bool exists(const std::string& path) override
    {
        return session.isValid() && session.containsEntry(juce::String::fromUTF8(path.c_str()));
    }

    bool read(const std::string& path, std::string& text) override
    {
        if (! session.isValid())
            return false;
        juce::MemoryBlock data;
        if (! session.readEntry(juce::String::fromUTF8(path.c_str()), data))
            return false;
        text.assign(static_cast<const char*>(data.getData()), data.getSize());
        return true;
    }

    bool write(const std::string& path, const std::string& text) override
    {
        if (! session.isValid())
            return false;
        return session.writeEntry(juce::String::fromUTF8(path.c_str()), juce::MemoryBlock(text.data(), text.size()));
    }

    bool remove(const std::string& path) override
    {
        return session.isValid() && session.removeEntry(juce::String::fromUTF8(path.c_str()));
    }

    std::vector<std::string> list(const std::string& prefix) override
    {
        std::vector<std::string> found;
        if (! session.isValid())
            return found;
        const auto wanted = juce::String::fromUTF8(prefix.c_str());
        for (const auto& path : session.listEntryPaths())
            if (path.startsWith(wanted))
                found.push_back(path.toStdString());
        return found;
    }

private:
    creation::assets::ProjectSession& session;
};
