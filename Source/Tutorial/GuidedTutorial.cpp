#include "GuidedTutorial.h"
#include "TutorialScriptCompiler.h"

namespace cw::tutorial
{
namespace
{
Action makeAction(ActionType type, juce::String value)
{
    Action action;
    action.type = type;
    action.value = std::move(value);
    return action;
}

Scene makeScene(juce::String id,
                juce::String title,
                juce::String body,
                juce::String targetId,
                bool advanceOnTargetClick,
                bool drawConnector,
                juce::String nextButtonText,
                juce::Array<Action> actions)
{
    Scene scene;
    scene.id = std::move(id);
    scene.title = std::move(title);
    scene.body = std::move(body);
    scene.targetId = std::move(targetId);
    scene.advanceOnTargetClick = advanceOnTargetClick;
    scene.drawConnector = drawConnector;
    scene.nextButtonText = std::move(nextButtonText);
    scene.actions = std::move(actions);
    return scene;
}
}

juce::String getBuiltInGettingStartedTutorialSource()
{
    return R"NALM(
tutorial getting-started-demo "Getting Started Demo"
description "A recordable guided walkthrough of the core Creation Station workspaces."

scene welcome
title "Welcome"
say "This guided demo walks the app for you so you can record a clean lesson. Start OBS first, then click through while the overlay points out what matters."
advanceOnClick false
connector false
next "Begin Demo"
do switch-workspace arrange
endscene

scene transport
title "Transport"
say "Use Play, Stop, and Record here. This is the fastest place to audition and capture ideas."
focus transport
do switch-workspace arrange
endscene

scene modes
title "Creative Modes"
say "These modes switch between Foley staging, signal forging, content browsing, layering, patch design, scripting, capture, and AI help."
focus modes
do switch-workspace arrange
endscene

scene signal
title "Signal Lab"
say "Signal Lab lets you design a tone from oscillators, noise, and an envelope, then inspect it with a scope and frequency analyzer. This is a strong place to demonstrate sound construction on screen."
focus signal
advanceOnClick false
next "Next Scene"
do switch-workspace signal
do apply-signal-template "Triangle Lead"
endscene

scene library
title "Library"
say "The content library is where free, downloaded, premium, and personal content will show up once the LagDaemon service is connected."
focus library
advanceOnClick false
next "Next Scene"
do switch-workspace library
endscene

scene layers
title "Layers"
say "The layer view is where raw sounds stack, blend, mute, solo, and move under the master strip."
focus mix
advanceOnClick false
next "Next Scene"
do switch-workspace mix
endscene

scene plugins
title "Plugin Studio"
say "Plugin Studio is where you point Creation Station at your VST folders, rescan the catalog, load inserts, and assign a plugin to the patch graph. This keeps plugin work on-screen instead of buried in popups."
focus plugins
advanceOnClick false
next "Next Scene"
do switch-workspace plugins
endscene

scene patch
title "Patch Lab"
say "This view is for sound-design chains: sources, processors, and printable outputs. The VST Host node now sits in the real signal path, so a third-party plugin can become part of the patch itself with live wet-dry control."
focus patch
advanceOnClick false
next "Next Scene"
do switch-workspace node
do apply-graph-macro "Instrument Voice"
endscene

scene script
title "Script"
say "The script view lets you write functional-style signal flow and automation as text."
focus code
advanceOnClick false
next "Next Scene"
do switch-workspace code
endscene

scene capture
title "Capture"
say "Capture mode arms source lanes and writes fresh takes into your project folder."
focus record
advanceOnClick false
next "Next Scene"
do switch-workspace record
endscene

scene ai
title "AI Guided Work"
say "The AI workspace now builds a structured plan, carries pipeline data when needed, and can drive the next visible action in the app. This is the view to use when teaching how the assistant collaborates rather than just chats."
focus ai
advanceOnClick false
next "Finish Demo"
do switch-workspace ai
endscene

scene done
title "Done"
say "That is the guided map. You can record this walkthrough with OBS now, then layer in your own voice-over later."
focus transport
advanceOnClick false
next "Done"
do switch-workspace arrange
endscene
)NALM";
}

juce::String getBuiltInVstNodeDemoTutorialSource()
{
    return R"NALM(
tutorial vst-node-demo "VST Node Demo"
description "A focused walkthrough for showing Plugin Studio and the VST Host node working together."

scene intro
title "VST Node Demo"
say "This short demo is built for showing how third-party plugins move from the library of discovered VSTs into the patch graph."
advanceOnClick false
connector false
next "Start Demo"
do switch-workspace plugins
endscene

scene pluginstudio
title "Plugin Studio"
say "Start here. This workspace keeps VST folders, scans, plugin browsing, and graph assignment visible on one screen."
focus plugins
advanceOnClick false
next "Next Scene"
do switch-workspace plugins
endscene

scene patchlab
title "Patch Lab"
say "Now the VST Host node sits inside the actual sound path. Once a plugin is assigned, its output is blended with the graph using the node mix control."
focus patch
advanceOnClick false
next "Next Scene"
do switch-workspace node
do apply-graph-macro "Instrument Voice"
endscene

scene finish
title "Demo Complete"
say "For the live demo: assign a favorite VST in Plugin Studio, switch back to Patch Lab, and move the VST Host mix control while audio is playing."
focus patch
advanceOnClick false
next "Done"
do switch-workspace node
endscene
)NALM";
}

Script makeGettingStartedTutorial()
{
    auto builtInTutorialScript = getBuiltInGettingStartedTutorialSource();

    ScriptCompiler compiler;
    Script script;
    juce::String errorMessage;
    if (compiler.compile(builtInTutorialScript, script, errorMessage))
        return script;

    Script fallback;
    fallback.id = "tutorial-fallback";
    fallback.name = "Tutorial Fallback";
    fallback.description = errorMessage;
    fallback.scenes.add(makeScene("fallback",
                                  "Tutorial Script Error",
                                  errorMessage,
                                  {},
                                  false,
                                  false,
                                  "Done",
                                  { makeAction(ActionType::switchWorkspace, "arrange") }));
    return fallback;
}
}
