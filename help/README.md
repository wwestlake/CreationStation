# Djehuti Station help sources

Structured help for Djehuti Station, written for two readers from one source: a help browser and the Virtual Engineer
(the in-app AI assistant). It follows the help-system design in the research prototype
(`projects/djehuti-suite-help` in the lagdaemon-tech-research repository): help topics are JSON, application features
have stable `helpId` values, and each topic records the behavior signature it was reviewed against.

## What is here

- `topics/en-US/*.json` - 48 topics (tutorial, how-to, reference, explanation, troubleshooting), one file each. Topic
  IDs are `djehuti.station.<subject>`. This is the canonical source: edit these files.
- `help-inventory.json` - an inventory of Station's public features (89 items: panels, menu commands, transport
  controls, workflows, settings, errors), each with a stable `helpId` and a `behaviorSignature`.
- `inventory-descriptors.json` - the behavior descriptor each signature was computed from.

## How Station uses it

The topics and inventory are compiled into the executable (`StationHelpData` in CMakeLists.txt); nothing is read from
the computer at run time. `Source/Help/HelpLibrary` loads them and serves both readers:

- **Help window** (`Source/Help/HelpPanel`): Help > Help Topics, or F1 for the panel you are in. Search, topic list,
  rendered topic, related-topic buttons. Works with no AI account.
- **Virtual Engineer** (`MainComponent::launchAiCompletion`): each question gets the topics for the panel the user is in
  plus the best search matches, labelled with their topic ids, and the system prompt tells the assistant to answer from
  them, name the topic, and say so when the help does not cover the question.

`tests/HelpLibrarySmoke.cpp` (target `HelpLibrarySmoke`) checks that the embedded help loads, is consistent, and finds
the right topics. Editing a topic or the inventory needs a rebuild to take effect. The panel-to-help-ID mapping for F1 is
`MainComponent::currentHelpId`.

## Status: draft, written from the source

Every topic is `status: "draft"`. They were written on 2026-09-20 by reading Station's source (menus, panels, dialogs,
strings, tests), not by a person walking through the running application, and are recorded as reviewed by
"Claude (from Station source, not yet reviewed by a person)". A topic should be promoted to `verified` only after a
person has checked it against the running application.

The inventory is hand-authored (interim). In the design the application generates it from its own command and view
registries; until that exists, `inventory-descriptors.json` holds the descriptors and the signatures were computed from
them (SHA-256 of the canonical JSON: sorted keys, no whitespace).

## Validate and compile

The compiler is `tools/build_help.py` in the research prototype and needs Python 3.8+ with `jsonschema`. From the
prototype directory:

    python tools/build_help.py --topics <this dir>/topics --inventory <this dir>/help-inventory.json --output <build dir>

It exits nonzero for schema errors, uncovered required `helpId`s, stale behavior signatures, broken relations or
duplicate IDs, and writes `help-catalog.json`, `context-map.json`, `semantic-cards.jsonl` (for the Virtual Engineer) and
`sync-report.json`. At the time of writing it compiles 48 topics into 274 semantic cards with no errors or warnings.

## When Station changes

If a documented command, panel or workflow changes what a user sees or can do: update its descriptor in
`inventory-descriptors.json`, regenerate its signature, review the topics that list that `helpId` under `contexts`,
correct them, and record the new signature in each context's `verifiedBehaviorSignature`. Bump `contentRevision` and
`reviewedAt` for topics you reviewed.

## Known gaps

- Not documented yet, or only lightly: the Score panel's AI coach, MIDI editing, the Suite Log and VFS Browser tabs of
  the suite settings, the Sign In flow, and the Content Library / tutorial library.
- The Guided Tour's built-in script is out of date (it still says "Creation Station", describes a mode strip that no
  longer exists, and says captures are written to "your project folder"); the topic says to trust the help over the tour.
- Video: rendering or exporting a finished video is not built yet and is described as such.
