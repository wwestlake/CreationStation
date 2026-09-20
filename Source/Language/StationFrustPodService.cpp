#include "StationFrustPodService.h"

#include <creation/frust/FratePodVfsResolver.h>
#include <creation/frust/NodeLibraryLoader.h>
#include <creation/frust/SuiteFratePodWorkspace.h>

#include <frate/FrateConfig.h>
#include <frate/PodBuild.h>

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

bool StationFrustPodService::loadPodFromMemory(const std::string& key,
                                               const frate::PodFiles& pod,
                                               frate::PodSource& dependencies,
                                               juce::String& error) {
    frate::PodEntry entry;
    std::string entryError;
    if (!frate::findPodEntry(pod, entry, entryError)) {
        error = juce::String(entryError);
        return false;
    }

    // The plugin runtime compiles the pod's own entry text; the files its
    // `use self::x;` lines name and the pods it uses come out of memory too.
    frust::CompileRequest request;
    request.sources.push_back({ key + "/" + entry.name, entry.text });
    request.siblingFiles = [&pod](const std::string& name, std::string& text) {
        const auto found = pod.find(name);
        if (found == pod.end()) return false;
        text = found->second;
        return true;
    };
    frate::FrateConfig config;
    config.loadFromString(pod.at("frate.json"));
    request.pods = [&config, &dependencies](const std::string& name, const std::string& requested,
                                            frust::PodSource& out) {
        std::string version = requested;
        if (version.empty() || version == "current") {
            version.clear();
            for (const auto& dep : config.getDependencies())
                if (dep.name == name) { version = dep.version; break; }
            if (version.empty()) return false;
        }
        frate::PodFiles files;
        if (!dependencies.findPod(name, version, files)) return false;
        frate::FrateConfig depConfig;
        const auto manifest = files.find("frate.json");
        if (manifest != files.end() && depConfig.loadFromString(manifest->second))
            out.ns = depConfig.getMetadata().namespacePath;
        for (const auto& [path, contents] : files)
            if (path.rfind("src/", 0) == 0) out.sources.push_back({ path.substr(4), contents });
        return true;
    };

    // Sibling names arrive relative to the entry file's folder ("src/").
    const auto inPod = request.siblingFiles;
    request.siblingFiles = [inPod](const std::string& name, std::string& text) {
        return inPod("src/" + name, text) || inPod(name, text);
    };

    std::string runtimeError;
    runtime_.unload(key);
    if (!runtime_.loadSource(key, request, runtimeError)) {
        error = juce::String(runtimeError);
        return false;
    }
    return true;
}

bool StationFrustPodService::buildGeneratedNodePod(creation::assets::ProjectSession& session,
                                                    const juce::String& podName,
                                                    const juce::String& generatedSource,
                                                    ce::node_system::NodeLibraryRegistry& nodeLibraries,
                                                    juce::String& status) {
    if (!ensureVfs(status)) return false;

    creation::frust::FratePodVfsResolver resolver(vfsClient_, registryClient_);
    creation::frust::SuitePodSource podSource(session, resolver);
    creation::frust::SuiteFratePodWorkspace workspace(session, podSource);
    creation::frust::PodScaffoldOptions options;
    options.name = podName;
    options.intendedApplication = "djehuti-station";
    if (!workspace.writeSource(options, wrapAsPluginSource(podName, generatedSource), status)) return false;

    const auto result = workspace.build(podName);
    if (!result.success) {
        status = result.output;
        return false;
    }

    const auto key = podName.toStdString();
    juce::String loadError;
    if (!loadPodFromMemory(key, result.files, podSource, loadError)) {
        status = "Built and packaged, but JIT loading failed: " + loadError;
        return false;
    }
    if (!registerLoadedLibraries(key, nodeLibraries, status)) return false;

    status = "Built, packaged, loaded, and registered " + podName + " from project VFS.";
    return true;
}

bool StationFrustPodService::loadRegistryNodePod(creation::assets::ProjectSession& session,
                                                  const juce::String& podName,
                                                  const juce::String& version,
                                                  ce::node_system::NodeLibraryRegistry& nodeLibraries,
                                                  juce::String& status) {
    if (!ensureVfs(status)) return false;
    creation::frust::FratePodVfsResolver resolver(vfsClient_, registryClient_);
    frate::PodFiles pod;
    const auto resolved = resolver.resolve(podName.toStdString(), version.toStdString(), pod);
    if (resolved != creation::frust::PodResolveStatus::ResolvedFromRegistry
        && resolved != creation::frust::PodResolveStatus::ResolvedFromVfsCache) {
        status = "Could not resolve " + podName + " " + version + " from the Frate registry.";
        return false;
    }

    creation::frust::SuitePodSource podSource(session, resolver);
    const auto key = (podName + "@" + version).toStdString();
    juce::String loadError;
    if (!loadPodFromMemory(key, pod, podSource, loadError)) {
        status = "Pod resolved but JIT loading failed: " + loadError;
        return false;
    }
    if (!registerLoadedLibraries(key, nodeLibraries, status)) return false;
    status = "Loaded " + podName + " " + version + " and registered its reflected nodes.";
    return true;
}

} // namespace cw
