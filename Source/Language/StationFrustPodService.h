#pragma once

#include <creation/assets/ProjectSession.h>
#include <creation/frust/PluginRuntime.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <node_system/node_library.h>

#include <frate/FrateRegistryClient.h>
#include <frate/PodArchive.h>
#include <frate/PodBuild.h>

namespace cw {

// Station's FRust pods, held entirely in the project VFS and the Suite pod cache
// (also in the VFS). Building compiles the pod in memory; loading hands its source
// text straight to the plugin runtime. No folder is created, no temporary file
// is written, and no other program (frate, the compiler) is started.
class StationFrustPodService final {
public:
    StationFrustPodService();

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
    bool loadPodFromMemory(const std::string& key,
                           const frate::PodFiles& pod,
                           frate::PodSource& dependencies,
                           juce::String& error);
    static juce::String wrapAsPluginSource(const juce::String& podName,
                                           const juce::String& generatedSource);
};

} // namespace cw
