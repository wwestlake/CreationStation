# Agent Instructions

## Build Output

- Always build Creation Station in the existing `D:\000 Creation Station\build` directory.
- The runnable app path must remain `D:\000 Creation Station\build\CreativeWorkstation_artefacts\Release\Creative Workstation.exe`.
- Do not create alternate or scratch build folders such as `build-asio`, `build-cleancheck`, `build-phase4`, or similar.
- If a different build directory ever seems necessary, stop and discuss it with the user before doing anything.

## Live Testing Pace

- The user tests hardware/audio manually (MIDI controllers, keyboards, sound) and reports results in their own words, at their own pace, often across several messages.
- Do not fire off another test, another question, or a next step after every single message. Wait until the user explicitly says they're done sending results before proposing what to do next.
- Do not stack multiple test requests or clarifying questions back to back. One at a time, and only when actually stuck after investigating what's checkable without the user's hands-on involvement.
- The user is not an LLM and cannot process rapid-fire requests the way you can. Slow down.

## GitHub Wiki

- The `wiki/` directory is intentionally ignored by the main code repository.
- Treat the wiki as its own separate Git repository, matching how GitHub wikis work.
- Do not force-add, stage, commit, or push files from `wiki/` as part of the main application repo.
- If wiki content needs to be published, handle it through the wiki repository/workflow only.
