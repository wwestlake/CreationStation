#pragma once

#include <creation/assets/ProjectSession.h>
#include <creation/frust/PluginRuntime.h>
#include <creation/frust/SuiteFrust.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <node_system/node_library.h>

#include <frate/FrateRegistryClient.h>

namespace cw {

// Station's use of FRust and Frate. The compiler and Frate are libraries linked into the app;
// SuiteFrust gives them the project VFS as their file system and log. Nothing here creates a
// folder, writes a temporary file, or starts another program.
class StationFrustPodService final {
public:
    StationFrustPodService();

    // The Script panel: stores the text in the project VFS and compiles it from there (check only).
    creation::frust::FrustOutcome checkScript(creation::assets::ProjectSession& session, const juce::String& source);

    bool buildGeneratedNodePod(creation::assets::ProjectSession& session,
                               const juce::String& podName,
                               const juce::String& generatedSource,
                               ce::node_system::NodeLibraryRegistry& nodeLibraries,
                               juce::String& status);

    bool loadRegistryNodePod(creation::assets::ProjectSession& session,
                             const juce::String& podName,
                             const juce::String& version,
                             ce::node_system::NodeLibraryRegistry& nodeLibraries,
                             juce::String& status);

private:
    creation::services::SuiteVfsServiceClient vfsClient_;
    frate::FrateRegistryClient registryClient_;
    creation::frust::PluginRuntime runtime_ { "djehuti-station" };

    bool ensureVfs(juce::String& error);
    bool registerLoadedLibraries(const std::string& key,
                                 ce::node_system::NodeLibraryRegistry& destination,
                                 juce::String& error);
    static juce::String wrapAsPluginSource(const juce::String& podName,
                                           const juce::String& generatedSource);
};

} // namespace cw
