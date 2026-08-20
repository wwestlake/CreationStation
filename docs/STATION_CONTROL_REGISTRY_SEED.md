# Creation Station — Control Registry Seed List

Concrete `set_state`/`get_state` entries (see `shared/CEL/docs/
CEL_V2_LANGUAGE_SPEC.md` section 6 for the mechanism) for Creation
Station, seeded from a real investigation of the current codebase
(2026-08-02), not guessed. Every entry below cites the exact getter/
setter it should be wired to, or flags that one needs to be added first.

This is a seed list, not a final API — review before implementing;
some naming/grouping decisions below are proposals, not settled.

## Naming convention

Reuses the existing precedent already in this codebase —
`ControlSurfaceMappingStore`'s `targetId` strings (`"transport_play"`,
`"transport_record"`, etc., `Source/ControlSurface/
ControlSurfaceMappingStore.h`) — rather than inventing a second
convention. Dotted, lowercase, singular nouns: `track.gain`,
`transport.tempo`, `master.gain`.

## Authoritative source per property, stated explicitly

Two parallel track models exist and must not be confused: `TimelineTrack`
(`Source/Timeline/TimelineModel`) owns identity/name/kind/arrangement;
`WorkstationAudioEngine::TrackChannelSource` owns live mixer/routing/
plugin state. Each entry below states which one the registry handler
should actually call into. Where both exist (name, in particular),
`MainComponent` currently keeps them in sync manually (e.g.
`Source/MainComponent.cpp:1887`) — a registry handler for `track.name`
should set both, or `TimelineModel` should become sole authority with
the engine's copy becoming a lookup, not two independent values kept in
sync by hand. Not decided here.

## Per-track properties (form: `set_state(target: entity, name, value)`)

| Registry name | Type | Authoritative API | Status |
|---|---|---|---|
| `track.gain` | float | `WorkstationAudioEngine::setTrackGain`/`getTrackGain` (`.h:218,223`, `.cpp:2647,2679`) | Ready |
| `track.pan` | float | `setTrackPan`/`getTrackPan` (`.h:219,224`, `.cpp:2655,2685`) | Ready |
| `track.muted` | bool | `setTrackMuted`/`isTrackMuted` (`.h:220,225`, `.cpp:2663,2691`) | Ready |
| `track.soloed` | bool | `setTrackSoloed`/`isTrackSoloed` (`.h:221,226`, `.cpp:2671,2697`) | Ready |
| `track.armed` | bool | `setTrackRecordingArmed`/`isTrackRecordingArmed` (`.h:160-161`, `.cpp:1346,1352`) | Ready |
| `track.monitoring` | bool | `setTrackMonitoringEnabled`/`isTrackMonitoringEnabled` (`.h:162-163`, `.cpp:1360,1366`) | Ready |
| `track.stereo` | bool | `setTrackStereoEnabled`/`isTrackStereoEnabled` (`.h:164-165`, `.cpp:1374,1380`) | Ready |
| `track.input_channel` | int | `setTrackInputChannel`/`getTrackInputChannel` (`.h:159,158`, `.cpp:1340,1208`) | Ready |
| `track.midi_input_channel` | int | `setTrackMidiInputChannel`/`getTrackMidiInputChannel` (`.h:170-171`, `.cpp:1216,1222`) | Ready |
| `track.name` | string | Engine: `setTrackName`/`getTrackName` (`.h:214-215`, `.cpp:2624,2616`). TimelineModel: `setTrackName`/`getTrackName` (`TimelineModel.h:133-134`, `.cpp:450,459`) | Ready, but see "kept in sync manually" note above — pick one authority |
| `track.kind` | string (`"audio"`/`"midi"`/`"automation"`/`"signal"`/`"foley"`/`"folder"`/`"marker"`) | `TimelineModel::setTrackKind`/`getTrackKind` (`.h:139-140`, `.cpp:475,629`) | Ready on TimelineModel; engine side only has 2 independent bools (`isMidiKind`/`isAutomationKind`) — reconcile before wiring the setter, see note below |
| `track.level` (live input meter) | float | `getTrackLevel` (`.h:216`, `.cpp:2630`) | Ready, read-only (no setter needed) |
| `track.recording_peak` | float | `consumeTrackRecordingPeak` (`.h:217`) — read-and-clear semantics | Ready, read-only; note the consume-on-read behavior before wiring `get_state` |
| `track.channel_mode` | string (`"mono"`/`"stereo"`) | `TimelineModel::setTrackChannelMode`/`getTrackChannelMode` (`.h:141-142`, `.cpp:637,645`) | Ready |
| `track.parent` | int (track index, -1 = top-level) | `TimelineModel::setTrackParent`/`getTrackParent` (`.h:147-148`, `.cpp:492,524`); engine mirror `setTrackParentIndex` (`.h:200`, `.cpp:1940`, setter only) | Ready to set; engine side has no public getter — add one, or read via TimelineModel only |
| `track.folded` (UI collapse) | bool | none | **Needs new getter/setter on `TimelineModel`** — field exists (`TimelineTypes.h:100`), no accessor |

**`track.kind` reconciliation, flagged not resolved**: `TimelineTrack::kind` is a real 7-way enum; the engine only tracks "is MIDI" / "is automation" as two independent bools, with "audio" as the implicit default. A `set_state("track.kind", "foley")` call has nowhere sensible to go on the engine side today. Recommend: `TimelineModel` becomes the sole read authority for kind (already true for name/identity), and a setter is added there if scripted track-kind changes are actually wanted — don't add a third representation to reconcile.

## Plugin control (form: `set_state(target: entity, name, value)`, target = track)

| Registry name | Type | Authoritative API | Status |
|---|---|---|---|
| `track.plugin.load` (action, not strictly "state" — see below) | string (plugin name, resolved via catalog) | `loadTrackPlugin(trackIndex, file, err)` (`.h:263`, `.cpp:2830`) — needs a name→`juce::File` lookup through `VstPluginCatalog::getEntries()` (`VstPluginCatalog.h:32-35`) first | Ready, but this is a command (creates a plugin instance), not a plain value-set — likely belongs with `create_track`/`add_plugin`-style real intrinsics per the state-vs-action split, not literally `set_state` |
| `track.plugin.bypassed` | bool | `setTrackPluginBypassed`/`isTrackPluginBypassed` (`.h:274-277`, `.cpp:3098-3123`) — per-slot | Ready (needs a slot index too — three-argument form, or a fourth `set_state` overload; not decided) |
| `track.plugin.parameter` | float | `getTrackPluginParameterValue`/`setTrackPluginParameterValueRealtime` (`.h:288-291`, `.cpp:2914-2938`) — audio-thread-safe by design, a good sign for eventually allowing this even from a tighter execution context | Ready, same slot/param-index addressing question as above |
| `track.plugin.count` | int | `getTrackPluginCount` (`.h:273`, `.cpp:3090`) | Ready, read-only |
| `track.plugin.name` | string | `getTrackPluginName`/`getTrackPluginNames` (`.h:268-269`, `.cpp:2866,2874`) | Ready, read-only |

**Open question, not decided**: plugin control needs to address track *and* slot (a track can have a plugin chain, not just one plugin). `set_state`'s two forms (global / single-target) don't cleanly cover "target + sub-target." Either extend to a three-argument-plus form, or treat per-slot plugin parameters as their own bespoke intrinsics rather than forcing them through `set_state`.

## Global / transport properties (form: `set_state(name, value)`)

| Registry name | Type | Authoritative API | Status |
|---|---|---|---|
| `transport.tempo` | float (BPM) | `TimelineModel::setTempo(bpm, num, denom)`/`getTempoBpm()` (`.h:56-57`, `.cpp:14`) | Ready — note `setTempo` also takes time signature in the same call, see next row |
| `transport.time_signature` | (int numerator, int denominator) — two values, doesn't fit a single-`T` `set_state` cleanly | `setTempo(bpm, num, denom)` (same call as tempo); getters `getTimeSignatureNumerator/Denominator` (`.h:58-59`) | Needs either a combined struct-ish value or two separate registry entries (`transport.time_signature_numerator`/`_denominator`) calling into the one combined setter — not decided |
| `transport.musical_key` | string | `setMusicalKey`/`getMusicalKey` (`.h:60-61`) | Ready |
| `transport.playing` | bool | `WorkstationAudioEngine::setPlaying`/`isPlaying` (`.h:72-73`, `.cpp:2113`) | Ready |
| `transport.recording` | bool | `isRecording()` (`.h:78`) — read-only; actually starting/stopping goes through `startRecordingToFile`/`stopRecording` (`.h:80-82`), a command not a plain value-set | Read via `get_state`; the write side is an action, not `set_state` |
| `transport.position_seconds` | float | `TimelineModel::setTransportSeconds`/`getTransportSeconds` (`.h:68-69`, `.cpp:42`); engine mirror `setPlaybackPositionSeconds` (`.h:74`, `.cpp:2126`) | Ready |
| `transport.loop_enabled` | bool | `setLoopEnabled`/`isLoopEnabled` (`.h:116-117`) | Ready |
| `transport.loop_start_seconds` / `transport.loop_end_seconds` | float / float | `setLoopRegion`/`getLoopStartSeconds`/`getLoopEndSeconds` (`.h:109-115`, `.cpp:705`) | Ready |
| `transport.click_enabled` | bool | `setMetronomeEnabled`/`isMetronomeEnabled` (`.h:75-76`, inline) | Ready |
| `master.gain` | float | `setMasterGain`/`getMasterGain` (`.h:227-228`, `.cpp:2703`) | Ready |
| `master.plugin.bypassed` | bool | `setMasterPluginBypassed`/`isMasterPluginBypassed` (`.h:259-260`, `.cpp:2815,2820`) | Ready |

## Foley — genuine gaps, not a seed list yet

No dedicated Foley class or per-property API exists. `ArrangeView`
exposes read-only getters for a selected clip's trim/gain/fade/reverse/
normalize (`getTrimStart/getTrimEnd/getGainDecibels/
getFadeInNormalized/getFadeOutNormalized/isReverseEnabled/
isNormalizeEnabled`, `ArrangeView.h:39-45`) but **no public setters** —
mutation only happens internally via GUI-driven
`applyEditorValuesToSelectedClip()` (`.h:224`). Loading a whole Foley
arrangement is a bulk `juce::ValueTree` operation
(`WorkstationAudioEngine::setFoleyArrangement`, `.h:88`, `.cpp:2159-
2247`), not an incremental control surface.

**Before any Foley entry can be added to this registry, real setters
need to be added to `ArrangeView`** for at least: selected clip's trim
start/end, gain, fade in/out, reverse, normalize — mirroring the
existing getters. This is real, separate implementation work, not a
registry-wiring task like everything above.

**Also flagging a genuine bug-adjacent gap found during this
investigation**: Foley clip placement uses a hardcoded
`foleySecondsPerBeat = 0.5` (`WorkstationAudioEngine.cpp:102`, i.e. a
fixed 120 BPM grid), not `TimelineModel::getTempoBpm()`. If Foley
clips are meant to follow the project's actual tempo, this is already
wrong today, independent of anything CEL-related — worth its own fix
regardless of when/whether the control registry work happens.

## Explicitly out of scope for `set_state`/`get_state` — real intrinsics instead

Per the state-vs-action split in `CEL_V2_LANGUAGE_SPEC.md` section 6.1:
`create_track(name, kind) -> entity`, `remove_track(entity)`,
`add_plugin(track, pluginName) -> pluginHandle`,
`request_all_notes_off()` — these create/destroy things or are one-shot
commands, not named values with a current state.
