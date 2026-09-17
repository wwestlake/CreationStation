#pragma once

#include <creation/assets/ProjectSession.h>
#include <creation/frust/PluginRuntime.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <creation/suite/SuiteSettings.h>
#include <node_system/node_library.h>

#include <frate/FrateRegistryClient.h>

namespace cw {

class StationFrustPodService final {
public:
    StationFrustPodService();

    bool buildGeneratedNodePod(creation::assets::ProjectSession& session,
                               const creation::suite::SuiteSettings& settings,
                               const juce::String& podName,
                               const juce::String& generatedSource,
                               ce::node_system::NodeLibraryRegistry& nodeLibraries,
                               juce::String& status);

    bool loadRegistryNodePod(const creation::suite::SuiteSettings& settings,
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
    static juce::File cacheRoot(const creation::suite::SuiteSettings& settings);
    static juce::String wrapAsPluginSource(const juce::String& podName,
                                           const juce::String& generatedSource);
};

} // namespace cw
