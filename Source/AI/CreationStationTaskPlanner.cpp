#include "CreationStationTaskPlanner.h"

namespace
{
bool containsAny(const juce::StringArray& tokens, std::initializer_list<const char*> words)
{
    for (const auto* word : words)
        if (tokens.contains(word))
            return true;

    return false;
}
}

juce::StringArray CreationStationTaskPlanner::tokenize(const juce::String& text)
{
    auto normalized = text.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 _-@./");
    juce::StringArray tokens;
    tokens.addTokens(normalized, " \r\n\t", "");
    tokens.trim();
    tokens.removeEmptyStrings();
    tokens.removeDuplicates(false);
    return tokens;
}

CreationStationTaskPlanner::WorkflowKind CreationStationTaskPlanner::classifyPrompt(
    const juce::StringArray& tokens,
    const CreationStationContextEngine::ContextPacket& packet)
{
    if (containsAny(tokens, { "buzz", "hum", "noise", "ring", "ringing", "hiss", "notch", "fourier", "spectrum" }))
        return WorkflowKind::noiseReduction;

    if (containsAny(tokens, { "instrument", "synth", "oscillator", "envelope", "midi", "patch", "voice" }))
        return WorkflowKind::instrumentDesign;

    if (containsAny(tokens, { "foley", "record", "capture", "mic", "take", "sample" }))
        return WorkflowKind::captureSession;

    if (packet.request.workspaceMode.containsIgnoreCase("library")
        || containsAny(tokens, { "content", "pack", "preset", "publish", "library" }))
        return WorkflowKind::contentAssembly;

    return WorkflowKind::generic;
}

juce::String CreationStationTaskPlanner::stepTypeName(StepType type)
{
    switch (type)
    {
        case StepType::observe: return "Observe";
        case StepType::analyze: return "Analyze";
        case StepType::decide: return "Decide";
        case StepType::act: return "Act";
        case StepType::verify: return "Verify";
        case StepType::recover: return "Recover";
    }

    return "Observe";
}

juce::String CreationStationTaskPlanner::stepStatusName(StepStatus status)
{
    switch (status)
    {
        case StepStatus::pending: return "pending";
        case StepStatus::ready: return "ready";
        case StepStatus::blocked: return "blocked";
        case StepStatus::complete: return "complete";
    }

    return "pending";
}

juce::String CreationStationTaskPlanner::actionTargetName(ActionTarget target)
{
    switch (target)
    {
        case ActionTarget::workspace: return "Workspace";
        case ActionTarget::signalLab: return "Signal Lab";
        case ActionTarget::patchGraph: return "Patch Graph";
        case ActionTarget::transport: return "Transport";
        case ActionTarget::context: return "Context";
    }

    return "Context";
}

CreationStationTaskPlanner::TaskStep::Action CreationStationTaskPlanner::makeAction(ActionTarget target,
                                                                                   juce::String command,
                                                                                   juce::String label,
                                                                                   juce::String stringValue,
                                                                                   float floatValue,
                                                                                   bool boolValue)
{
    TaskStep::Action action;
    action.target = target;
    action.command = std::move(command);
    action.label = std::move(label);
    action.stringValue = std::move(stringValue);
    action.floatValue = floatValue;
    action.boolValue = boolValue;
    return action;
}

CreationStationTaskPlanner::TaskStep CreationStationTaskPlanner::makeStep(
    juce::String id,
    StepType type,
    juce::String title,
    juce::String detail,
    juce::String expectedResult,
    juce::String fallback,
    juce::StringArray tags,
    juce::Array<TaskStep::Action> actions)
{
    TaskStep step;
    step.id = std::move(id);
    step.type = type;
    step.title = std::move(title);
    step.detail = std::move(detail);
    step.expectedResult = std::move(expectedResult);
    step.fallback = std::move(fallback);
    step.tags = std::move(tags);
    step.actions = std::move(actions);
    step.status = StepStatus::ready;
    return step;
}

CreationStationTaskPlanner::TaskPlan CreationStationTaskPlanner::buildPlan(
    const juce::String& prompt,
    const CreationStationContextEngine::ContextPacket& packet) const
{
    TaskPlan plan;
    plan.goal = prompt.trim().isNotEmpty() ? prompt.trim() : "Untitled creative task";

    const auto tokens = tokenize(prompt);
    const auto workflow = classifyPrompt(tokens, packet);

    switch (workflow)
    {
        case WorkflowKind::noiseReduction:
            plan.workflow = "Noise Hunt";
            plan.summary = "Capture the problem, locate the offending band, apply a focused fix, then measure again.";
            plan.suggestedTools.addArray({ "Signal Lab", "Spectrum View", "Automation", "Patch Graph" });
            plan.verificationNote = "The buzz should drop in the spectrum and sound less intrusive without flattening the useful signal.";
            plan.dataSchema.add({ "analysis.sampleName", "Name of the captured noisy example", {}, 0.0, false });
            plan.dataSchema.add({ "analysis.primaryBuzzHz", "Strongest suspected buzz frequency", {}, 0.0, true });
            plan.dataSchema.add({ "analysis.primaryBuzzDb", "Approximate level of the strongest buzz peak", {}, 0.0, true });
            plan.dataSchema.add({ "treatment.strategy", "Chosen cleanup strategy", {}, 0.0, false });
            plan.steps.add(makeStep("observe-source", StepType::observe,
                                    "Capture the noisy source",
                                    "Listen to the source in isolation and collect a short example with the unwanted tone or buzz clearly present.",
                                    "A repeatable example is ready for analysis.",
                                    "If the noise is intermittent, loop a shorter section or raise monitoring temporarily.",
                                    { "capture", "source" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Signal Lab", "signal"),
                                      makeAction(ActionTarget::signalLab, "apply-template", "Seed Impact Tone for inspection", "Impact Tone") }));
            plan.steps.add(makeStep("analyze-spectrum", StepType::analyze,
                                    "Measure the frequency shape",
                                    "Run a Fourier or spectrum analysis pass to identify narrow peaks, harmonics, or low-end build-up linked to the buzz.",
                                    "One or more suspect frequency bands are identified.",
                                    "If no clear peak appears, compare quiet and noisy passages and look for the delta.",
                                    { "fft", "analysis" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Stay in Signal Lab", "signal"),
                                      makeAction(ActionTarget::signalLab, "preview-signal", "Preview current signal") }));
            plan.steps.add(makeStep("decide-treatment", StepType::decide,
                                    "Choose the least destructive fix",
                                    "Decide whether a notch, narrow EQ dip, high-pass, gain trim, or automation move gives the cleanest result.",
                                    "A specific correction strategy is chosen.",
                                    "If multiple bands compete, start with the strongest one and re-measure before stacking more cuts.",
                                    { "decision", "eq" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Patch workspace", "node"),
                                      makeAction(ActionTarget::patchGraph, "apply-macro", "Seed cleanup patch graph", "Noise Cleanup") }));
            plan.steps.add(makeStep("apply-fix", StepType::act,
                                    "Apply the correction",
                                    "Create or adjust the patch, filter, or automation to reduce the offending energy while preserving character.",
                                    "The signal path now includes the chosen repair.",
                                    "If the repair dulls the source, ease the depth or widen the context before changing more nodes.",
                                    { "dsp", "patch" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Return to Signal Lab", "signal"),
                                      makeAction(ActionTarget::signalLab, "apply-template", "Switch to a bright analysis source", "Triangle Lead") }));
            plan.steps.add(makeStep("verify-cleanup", StepType::verify,
                                    "Re-check by ear and by meter",
                                    "Compare before and after, confirm the buzz is lower, and verify the musical or expressive qualities still read correctly.",
                                    "The fix is confirmed or queued for another pass.",
                                    "If artifacts remain, branch into a second treatment cycle with tighter context.",
                                    { "validation", "listen" },
                                    { makeAction(ActionTarget::signalLab, "preview-signal", "Audition the updated signal") }));
            break;

        case WorkflowKind::instrumentDesign:
            plan.workflow = "Instrument Build";
            plan.summary = "Shape a playable sound source, define control movement, and validate it from MIDI or scripted triggers.";
            plan.suggestedTools.addArray({ "Signal Lab", "Patina", "Patch Graph", "Creation Station Instrument" });
            plan.verificationNote = "A note-on event should produce the intended sound shape with predictable dynamics and controllable tone.";
            plan.dataSchema.add({ "instrument.targetCharacter", "Plain-language sound target", {}, 0.0, false });
            plan.dataSchema.add({ "instrument.templateName", "Starting template selected for the voice", {}, 0.0, false });
            plan.dataSchema.add({ "instrument.graphMacro", "Patch graph macro selected for the build", {}, 0.0, false });
            plan.steps.add(makeStep("observe-intent", StepType::observe,
                                    "Define the target sound",
                                    "Describe the source character, pitch behavior, articulation, and emotional role of the instrument.",
                                    "The patch has a clear sonic brief.",
                                    "If the sound goal is fuzzy, compare it to a waveform family or a known reference patch.",
                                    { "brief", "sound-design" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Signal Lab", "signal"),
                                      makeAction(ActionTarget::signalLab, "apply-template", "Start from Soft Keys", "Soft Keys") }));
            plan.steps.add(makeStep("analyze-building-blocks", StepType::analyze,
                                    "Choose source and modulation blocks",
                                    "Select oscillators, envelopes, filters, and modulation paths that can produce the target contour.",
                                    "The core graph ingredients are identified.",
                                    "If the patch feels too broad, begin with one oscillator and one envelope only.",
                                    { "oscillator", "envelope" },
                                    { makeAction(ActionTarget::signalLab, "apply-template", "Swap to Triangle Lead for a brighter voice", "Triangle Lead") }));
            plan.steps.add(makeStep("decide-graph", StepType::decide,
                                    "Commit to a patch layout",
                                    "Lay out the signal flow from source to shaping to output and decide which parameters deserve exposure.",
                                    "A manageable first graph is locked in.",
                                    "If routing becomes complex, freeze a minimal voice path and defer extras.",
                                    { "graph", "routing" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Patch workspace", "node"),
                                      makeAction(ActionTarget::patchGraph, "apply-macro", "Seed instrument voice graph", "Instrument Voice") }));
            plan.steps.add(makeStep("build-patch", StepType::act,
                                    "Build the playable patch",
                                    "Create the patch in Patina or the node system and bind the main controls for level, tone, and articulation.",
                                    "A playable instrument patch exists.",
                                    "If the first build is unstable, reduce the graph to a single voice and add modules back one at a time.",
                                    { "patina", "instrument" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Script workspace", "code") }));
            plan.steps.add(makeStep("verify-playability", StepType::verify,
                                    "Test with notes and dynamics",
                                    "Trigger notes, listen for envelope shape, clipping, and response, and confirm the control surface or MIDI path behaves.",
                                    "The patch responds musically and predictably.",
                                    "If response is uneven, inspect gain staging and envelope timing before adding more features.",
                                    { "midi", "test" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Return to Signal Lab", "signal"),
                                      makeAction(ActionTarget::signalLab, "preview-signal", "Preview the playable source") }));
            break;

        case WorkflowKind::captureSession:
            plan.workflow = "Capture Session";
            plan.summary = "Prepare the source, record clean takes, and turn them into organized assets ready for shaping or reuse.";
            plan.suggestedTools.addArray({ "Capture", "Foley", "Content Library", "Layers" });
            plan.verificationNote = "The recorded take should be clean, named well, and immediately reusable in a patch or content pack.";
            plan.dataSchema.add({ "capture.sourceType", "Type of source being recorded", {}, 0.0, false });
            plan.dataSchema.add({ "capture.takeName", "Best captured take name", {}, 0.0, false });
            plan.steps.add(makeStep("observe-scene", StepType::observe,
                                    "Check the recording scene",
                                    "Confirm the source, room, mic path, and monitoring chain are ready for the take you want.",
                                    "The recording setup is understood before rolling.",
                                    "If the source is noisy, move or isolate it before trying to fix everything later.",
                                    { "recording", "prep" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Capture workspace", "record") }));
            plan.steps.add(makeStep("analyze-noise-floor", StepType::analyze,
                                    "Listen for background problems",
                                    "Check the idle signal for hum, fan noise, clipping risk, or level imbalance before the real take.",
                                    "The noise floor and headroom are known.",
                                    "If the floor is high, adjust gain or source placement first.",
                                    { "noise-floor", "gain" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Signal Lab", "signal"),
                                      makeAction(ActionTarget::signalLab, "apply-template", "Use Noisy Pluck as a quick transient test", "Noisy Pluck") }));
            plan.steps.add(makeStep("decide-take-plan", StepType::decide,
                                    "Set the take strategy",
                                    "Choose whether you need one hero take, a layered pass, or several variations for later editing.",
                                    "A practical recording plan is set.",
                                    "If unsure, capture three short variations rather than one long unfocused pass.",
                                    { "takes", "session" }));
            plan.steps.add(makeStep("capture-material", StepType::act,
                                    "Record and store the material",
                                    "Record the take, write it into the project storage root, and tag it so it can feed Signal Lab, Foley, or Library views.",
                                    "The source material is stored locally and organized.",
                                    "If the take is close but flawed, keep it and mark the flaw instead of discarding the context.",
                                    { "asset", "storage" }));
            plan.steps.add(makeStep("verify-reusability", StepType::verify,
                                    "Confirm the take is useful",
                                    "Play the result back in context and verify it is easy to find, audition, and reuse.",
                                    "The asset is production-ready.",
                                    "If retrieval feels clumsy, improve the naming and tags before recording more.",
                                    { "library", "review" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Library workspace", "library") }));
            break;

        case WorkflowKind::contentAssembly:
            plan.workflow = "Content Assembly";
            plan.summary = "Gather the asset, package its metadata, and verify it is ready for local use or publishing.";
            plan.suggestedTools.addArray({ "Content Library", "Project Assets", "Admin Publish" });
            plan.verificationNote = "The item should be well-described, versioned, and easy to activate locally or publish upstream.";
            plan.dataSchema.add({ "content.itemType", "Chosen content type", {}, 0.0, false });
            plan.dataSchema.add({ "content.version", "Content version prepared for delivery", {}, 0.0, false });
            plan.steps.add(makeStep("observe-asset", StepType::observe,
                                    "Identify the content unit",
                                    "Decide whether the item is a patch, sample pack, instrument preset, or another reusable artifact.",
                                    "The content type is clear.",
                                    "If it spans several formats, split the package into smaller units first.",
                                    { "content", "asset" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open Library workspace", "library") }));
            plan.steps.add(makeStep("analyze-metadata", StepType::analyze,
                                    "Review what the item needs",
                                    "Collect name, version, tags, tier, dependencies, and the minimum app expectations for the item.",
                                    "The package metadata is complete enough to ship.",
                                    "If versioning is uncertain, start with a pre-release and tighten later.",
                                    { "metadata", "version" }));
            plan.steps.add(makeStep("decide-delivery", StepType::decide,
                                    "Choose local-only or publish-ready",
                                    "Decide whether the item stays local, moves into the shared library, or is prepared for admin upload.",
                                    "The delivery path is chosen.",
                                    "If review is still needed, mark it as local draft and avoid publishing too early.",
                                    { "publish", "distribution" }));
            plan.steps.add(makeStep("package-item", StepType::act,
                                    "Write and stage the item",
                                    "Save the asset into the project content structure and prepare the file that will be published or activated.",
                                    "A package exists on disk in the expected location.",
                                    "If the file is missing pieces, regenerate from the source patch before uploading.",
                                    { "storage", "artifact" }));
            plan.steps.add(makeStep("verify-package", StepType::verify,
                                    "Test discovery and activation",
                                    "Refresh the library, confirm the item appears correctly, and verify it can be opened or activated.",
                                    "The content item behaves like a real product asset.",
                                    "If discovery fails, inspect the metadata and storage path before retrying.",
                                    { "refresh", "activation" }));
            break;

        case WorkflowKind::generic:
        default:
            plan.workflow = "Creative Iteration";
            plan.summary = "Frame the goal, gather context, make a focused change, and validate the result before expanding scope.";
            plan.suggestedTools.addArray({ "AI Context", "Signal Lab", "Patina", "Patch Graph" });
            plan.verificationNote = "Every pass should end with a concrete result or a tighter next question, never a vague blob.";
            plan.dataSchema.add({ "task.goalAnchor", "Restated goal anchor for the current pass", {}, 0.0, false });
            plan.steps.add(makeStep("observe-goal", StepType::observe,
                                    "Clarify the target",
                                    "Restate what success sounds like or does inside the project.",
                                    "The task has a crisp goal.",
                                    "If the ask is too broad, narrow it to one audible or structural win.",
                                    { "goal" },
                                    { makeAction(ActionTarget::workspace, "switch-mode", "Open AI workspace", "ai") }));
            plan.steps.add(makeStep("analyze-context", StepType::analyze,
                                    "Gather the strongest context",
                                    "Pull the most relevant local project, patch, and content references into view before changing anything.",
                                    "The next move is grounded in project reality.",
                                    "If context is weak, save the current patch or asset first so the AI has more to retrieve.",
                                    { "context" }));
            plan.steps.add(makeStep("decide-next-move", StepType::decide,
                                    "Choose the smallest useful action",
                                    "Pick the next transformation, patch edit, or automation pass that can prove progress quickly.",
                                    "One concrete action is selected.",
                                    "If several options compete, choose the most reversible one first.",
                                    { "decision" }));
            plan.steps.add(makeStep("act-on-plan", StepType::act,
                                    "Make the change",
                                    "Apply the chosen patch, script, signal, or content adjustment.",
                                    "The project moves forward in a visible way.",
                                    "If the result is noisy or unstable, roll back to the last clean state and try a smaller change.",
                                    { "action" }));
            plan.steps.add(makeStep("verify-result", StepType::verify,
                                    "Review the outcome",
                                    "Listen, inspect, and confirm whether the move improved the project enough to keep.",
                                    "The result is accepted or refined.",
                                    "If uncertainty remains, capture before and after notes for the next cycle.",
                                    { "review" }));
            break;
    }

    if (packet.dynamics.recoverySuggested)
    {
        plan.steps.add(makeStep("recover-context", StepType::recover,
                                "Stabilize the context before going wider",
                                "The prompt stream shows a strong pivot, so pause, restate the goal, and save the current state before branching.",
                                "The next pass starts from a clear anchor again.",
                                "If drift continues, split the work into a separate task instead of overloading the current one.",
                                { "recovery", "context" }));
    }

    return plan;
}

juce::String CreationStationTaskPlanner::describePlan(const TaskPlan& plan)
{
    juce::String text;
    text << "Task Plan\n\n";
    text << "Goal: " << plan.goal << "\n";
    text << "Workflow: " << plan.workflow << "\n";
    text << "Summary: " << plan.summary << "\n";

    if (! plan.suggestedTools.isEmpty())
        text << "Suggested tools: " << plan.suggestedTools.joinIntoString(", ") << "\n";

    if (! plan.dataSchema.isEmpty())
    {
        text << "Pipeline data:\n";
        for (const auto& entry : plan.dataSchema)
            text << "- " << entry.key << ": " << entry.description << "\n";
    }

    text << "\nSteps:\n";
    for (int index = 0; index < plan.steps.size(); ++index)
    {
        const auto& step = plan.steps.getReference(index);
        text << juce::String(index + 1) << ". "
             << step.title << " [" << stepTypeName(step.type) << ", " << stepStatusName(step.status) << "]\n";
        text << "   " << step.detail << "\n";
        text << "   Expect: " << step.expectedResult << "\n";
        text << "   Fallback: " << step.fallback << "\n";
        if (! step.actions.isEmpty())
        {
            text << "   Actions:\n";
            for (const auto& action : step.actions)
                text << "   - " << actionTargetName(action.target) << ": " << action.label << "\n";
        }
    }

    if (plan.verificationNote.isNotEmpty())
        text << "\nDone looks like: " << plan.verificationNote << "\n";

    return text;
}
