# Agent Instructions

## Build Output

- Always build Creation Station in the existing `D:\000 Creation Station\build` directory.
- The runnable app path must remain `D:\000 Creation Station\build\CreativeWorkstation_artefacts\Release\Creative Workstation.exe`.
- Do not create alternate or scratch build folders such as `build-asio`, `build-cleancheck`, `build-phase4`, or similar.
- If a different build directory ever seems necessary, stop and discuss it with the user before doing anything.

## GitHub Wiki

- The `wiki/` directory is intentionally ignored by the main code repository.
- Treat the wiki as its own separate Git repository, matching how GitHub wikis work.
- Do not force-add, stage, commit, or push files from `wiki/` as part of the main application repo.
- If wiki content needs to be published, handle it through the wiki repository/workflow only.
