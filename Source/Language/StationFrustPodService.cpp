#include "StationFrustPodService.h"

#include <creation/frust/FratePodVfsResolver.h>
#include <creation/frust/NodeLibraryLoader.h>
#include <creation/frust/SuiteFrateBuildService.h>
#include <creation/frust/SuiteFratePodWorkspace.h>
#include <creation/suite/SuiteStoragePaths.h>

#include <cmath>
#include <set>

namespace cw {
namespace {

void foleyPlaySample(const char*) {}
void foleyGainMix(double) {}
void foleyDelaySeconds(double) {}

juce::File podEntrySource(const juce::File& podDirectory) {
    const auto lib = podDirectory.getChildFile("src/lib.fr");
    return lib.existsAsFile() ? lib : podDirectory.getChildFile("src/main.fr");
}

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

juce::File StationFrustPodService::cacheRoot(const creation::suite::SuiteSettings& settings) {
    return creation::suite::getCacheDirectory(settings).getChildFile("FRust");
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

bool StationFrustPodService::buildGeneratedNodePod(creation::assets::ProjectSession& session,
                                                    const creation::suite::SuiteSettings& settings,
                                                    const juce::String& podName,
                                                    const juce::String& generatedSource,
                                                    ce::node_system::NodeLibraryRegistry& nodeLibraries,
                                                    juce::String& status) {
    if (!ensureVfs(status)) return false;

    const auto root = cacheRoot(settings);
    creation::frust::SuiteFrateBuildService builder(
        vfsClient_, registryClient_, juce::File(CS_FRATE_EXECUTABLE), root.getChildFile("registry-cache"));
    creation::frust::SuiteFratePodWorkspace workspace(session, builder, root.getChildFile("workspaces"));
    creation::frust::PodScaffoldOptions options;
    options.name = podName;
    options.intendedApplication = "djehuti-station";
    if (!workspace.writeSource(options, wrapAsPluginSource(podName, generatedSource), status)) return false;

    const auto result = workspace.build(podName);
    if (result.build.status != creation::frust::BuildStatus::Success) {
        status = result.build.output;
        return false;
    }

    const auto source = podEntrySource(result.materializedPodDirectory);
    std::string runtimeError;
    const auto key = podName.toStdString();
    runtime_.unload(key);
    if (!runtime_.load(key, source.getFullPathName().toStdString(), runtimeError)) {
        status = "Built and packaged, but JIT loading failed: " + juce::String(runtimeError);
        return false;
    }
    if (!registerLoadedLibraries(key, nodeLibraries, status)) return false;

    status = "Built, packaged, loaded, and registered " + podName + " from project VFS.";
    return true;
}

bool StationFrustPodService::loadRegistryNodePod(const creation::suite::SuiteSettings& settings,
                                                  const juce::String& podName,
                                                  const juce::String& version,
                                                  ce::node_system::NodeLibraryRegistry& nodeLibraries,
                                                  juce::String& status) {
    if (!ensureVfs(status)) return false;
    creation::frust::FratePodVfsResolver resolver(
        vfsClient_, registryClient_, cacheRoot(settings).getChildFile("registry-cache"));
    juce::File podDirectory;
    const auto resolved = resolver.resolve(podName.toStdString(), version.toStdString(), podDirectory);
    if (resolved != creation::frust::PodResolveStatus::ResolvedFromRegistry
        && resolved != creation::frust::PodResolveStatus::ResolvedFromVfsCache) {
        status = "Could not resolve " + podName + " " + version + " from the Frate registry.";
        return false;
    }
    const auto source = podEntrySource(podDirectory);
    if (!source.existsAsFile()) {
        status = "Resolved pod has no src/lib.fr or src/main.fr entry point.";
        return false;
    }
    const auto key = (podName + "@" + version).toStdString();
    runtime_.unload(key);
    std::string runtimeError;
    if (!runtime_.load(key, source.getFullPathName().toStdString(), runtimeError)) {
        status = "Pod resolved but JIT loading failed: " + juce::String(runtimeError);
        return false;
    }
    if (!registerLoadedLibraries(key, nodeLibraries, status)) return false;
    status = "Loaded " + podName + " " + version + " and registered its reflected nodes.";
    return true;
}

} // namespace cw
