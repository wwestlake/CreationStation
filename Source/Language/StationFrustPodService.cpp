#include "StationFrustPodService.h"

#include <creation/frust/NodeLibraryLoader.h>

#include <cmath>
#include <set>

namespace cw {
namespace {

void foleyPlaySample(const char*) {}
void foleyGainMix(double) {}
void foleyDelaySeconds(double) {}

} // namespace

StationFrustPodService::StationFrustPodService() {
    runtime_.registerHostFunction("sin", reinterpret_cast<void*>(static_cast<double (*)(double)>(std::sin)));
    runtime_.registerHostFunction("foley_play_sample", reinterpret_cast<void*>(foleyPlaySample));
    runtime_.registerHostFunction("foley_gain_mix", reinterpret_cast<void*>(foleyGainMix));
    runtime_.registerHostFunction("foley_delay_seconds", reinterpret_cast<void*>(foleyDelaySeconds));
}

bool StationFrustPodService::ensureVfs(juce::String& error) {
    if (vfsClient_.discover()) return true;
    error = "The Suite VFS service is unavailable.";
    return false;
}

juce::String StationFrustPodService::wrapAsPluginSource(const juce::String& podName,
                                                        const juce::String& generatedSource) {
    const auto manifest = "{\"name\":\"" + podName
        + "\",\"version\":\"0.1.0\",\"intendedApplications\":[\"djehuti-station\"]}";
    return "manifest " + juce::JSON::toString(juce::var(manifest), false) + ";\n\n" + generatedSource;
}

bool StationFrustPodService::registerLoadedLibraries(const std::string& key,
                                                      ce::node_system::NodeLibraryRegistry& destination,
                                                      juce::String& error) {
    ce::node_system::NodeLibraryRegistry loaded;
    std::string loadError;
    const std::set<std::string, std::less<>> capabilities {
        "audio", "foley", "station.audio", "station.foley"
    };
    if (!creation::frust::RegisterPluginNodeLibraries(runtime_.nodeLibraries(key), loaded,
                                                       capabilities, loadError)) {
        error = juce::String(loadError);
        return false;
    }
    for (const auto& [id, library] : loaded.Libraries()) {
        std::string replaceError;
        if (!destination.ReplaceLibrary(library, &replaceError)) {
            error = juce::String(replaceError);
            return false;
        }
    }
    return true;
}

creation::frust::FrustOutcome StationFrustPodService::checkScript(creation::assets::ProjectSession& session,
                                                                  const juce::String& source) {
    creation::frust::FrustOutcome outcome;
    if (!ensureVfs(outcome.output)) return outcome;
    creation::frust::SuiteFrust frust(session, vfsClient_, registryClient_);
    return frust.checkSource("Assets/Source/FRust/Scripts/patch.frust", source);
}

bool StationFrustPodService::buildGeneratedNodePod(creation::assets::ProjectSession& session,
                                                    const juce::String& podName,
                                                    const juce::String& generatedSource,
                                                    ce::node_system::NodeLibraryRegistry& nodeLibraries,
                                                    juce::String& status) {
    if (!ensureVfs(status)) return false;

    creation::frust::SuiteFrust frust(session, vfsClient_, registryClient_);
    creation::frust::PodScaffoldOptions options;
    options.name = podName;
    options.intendedApplication = "djehuti-station";
    if (!frust.writePodSource(options, wrapAsPluginSource(podName, generatedSource), status)) return false;

    const auto built = frust.buildPod(podName);
    if (!built.ok) {
        status = built.output;
        return false;
    }
    (void) frust.packagePod(podName);

    const auto key = podName.toStdString();
    juce::String loadError;
    if (!frust.loadAuthoredPod(runtime_, podName, podName, loadError)) {
        status = "Built and packaged, but JIT loading failed: " + loadError;
        return false;
    }
    if (!registerLoadedLibraries(key, nodeLibraries, status)) return false;

    status = "Built, packaged, loaded, and registered " + podName + " from the project VFS.";
    return true;
}

bool StationFrustPodService::loadRegistryNodePod(creation::assets::ProjectSession& session,
                                                  const juce::String& podName,
                                                  const juce::String& version,
                                                  ce::node_system::NodeLibraryRegistry& nodeLibraries,
                                                  juce::String& status) {
    if (!ensureVfs(status)) return false;

    creation::frust::SuiteFrust frust(session, vfsClient_, registryClient_);
    juce::String installError;
    if (!frust.installRegistryPod(podName, version, installError)) {
        status = installError;
        return false;
    }

    const auto key = (podName + "@" + version).toStdString();
    juce::String loadError;
    if (!frust.loadInstalledPod(runtime_, juce::String(key), podName, version, loadError)) {
        status = "Pod resolved but JIT loading failed: " + loadError;
        return false;
    }
    if (!registerLoadedLibraries(key, nodeLibraries, status)) return false;
    status = "Loaded " + podName + " " + version + " and registered its reflected nodes.";
    return true;
}

} // namespace cw
