# Shared project compatibility — Order 1

Station opens the same Suite VFS project as Movie. Its editing structures remain
in `session.xml`; opening a Movie-origin project with no Station session starts
from Station defaults. An invalid existing Station session is rejected before
replacing the current project. Movie's sequence data are never interpreted or
written by Station.

The existing project asset browser refreshes the shared catalog, preserving asset
and version identities and streaming compatible media from their existing VFS
paths. Catalog saves merge peer changes through the shared AssetSystem and the
updated VFS service. A Station session write failure retains dirty state.

The Debug `--shared-project-check <request.json>` harness exercises real project,
import, placement and save code, returns evidence and restores the startup
project before exiting. Its only purpose is the Suite Order 1 acceptance script.

Architecture, validation evidence and review dependencies are recorded in
[the Suite engineering report](../../../docs/architecture/Station-Movie-Shared-Project-Order-1.md).
