# Casting quarantine

Architecture approved quarantine under
[Suite work order 215](https://github.com/wwestlake/Creation-Suite/issues/215#issuecomment-6991394138).
Casting development and receiver testing are stopped. Windows wireless display
as an extended desktop is the supported TV workflow. Native title bars,
resizable windows, docking and detached windows remain part of Station.

## Shipping baseline

Canonical Station `master` at `757a8c5` already contains neither the experimental
casting module nor either casting menu entry. Its application source list and
test targets exclude casting. This quarantine retains that baseline. It does
not import the unmerged feature merely to disable it.

[Station PR 92](https://github.com/wwestlake/CreationStation/pull/92) contains the
unmerged casting feature and is outside the approved shipping baseline. Its
`Source/Casting/` module, `CastingSessionSmoke` and `CastingFeasibilityProbe`
targets, MainComponent lifecycle hooks, **Cast to TV** and **Wireless Display
(Fire TV / Miracast)** menu paths must remain excluded during reconciliation.

The module uses Windows casting, WGC/D3D11/DXGI, Media Foundation and network
streaming. Other Station video paths also use these Windows libraries; retain
their existing dependencies. No camera, receiver or casting behavior is being
changed or certified by this quarantine.

## Preserved experimental work

Before changing the checkout, the mixed local `develop` tip `1311f57` and all
14 uncommitted files were preserved in commit
`5378470382475cc646fccc02f016d01be44a61d3` on the local branch
`preserve/station-casting-wip-20261118`. The original `develop` ref is unchanged.
The snapshot retains the known casting resize compilation defect; it is not a
release candidate.

The durable complete-history archive is
[Station-Casting-Preservation-20261118.bundle](https://drive.google.com/file/d/1nfTek2943BT2nXuguRkOK9XEKT8vHTzW/view?usp=drivesdk)
in the existing private **Djehuti Suite** Drive folder. Drive metadata was checked
on November 18, 2026: owner-only access, no public/domain sharing. The link does
not change those permissions. The mixed source has not been published to GitHub.

- Size: 2,489,650 bytes.
- SHA-256: `cfd0082f41c3a506b3e212b65b63375008baac1803fc9ec733c1fdfd6f04eefe`.
- Preserved ref: `refs/heads/preserve/station-casting-wip-20261118`.
- Preserved tip: `5378470382475cc646fccc02f016d01be44a61d3`.

The uploaded archive was downloaded again, its SHA-256 matched, and
`git bundle verify` confirmed complete history. A verified local mirror and
checksum are also retained outside disposable build output in the workspace's
`ProjectArchives/CreationStation/` directory. The old build-folder bundle is a
working copy; archive durability does not depend on it or on a local Git ref.

To recover, download the restricted Drive file with the authorized account,
check its SHA-256, run `git bundle verify` in the existing repository, and inspect
`git bundle list-heads` before restoring the preserved ref. Do not use the mixed
snapshot as an ordinary feature PR or shipping baseline.

Valid non-casting project work remains separately reviewable in
[Station PR 94](https://github.com/wwestlake/CreationStation/pull/94), with its
Suite service dependency. It must not be imported through the casting branch.

Permanent deletion, dependency removal and any resumption of casting require a
later focused decision. Station–Movie integration, shared capture, MIDI
extraction and clock changes are outside this quarantine.

## Validation

The canonical single-worker Debug build of CreativeWorkstation,
LinkedClipsSmoke and SignalClipRenderSmoke passes on this master-based branch.
Both offline smoke checks pass with zero failures, and the Suite requirements
audit passes. The regenerated app project/solution excludes casting sources and
probes and retains ASIO. Runtime, windowing, build and test sources match master.

Checks used the existing shared workspace at Suite `85bf0ec`; reconciliation of
the shared-service baseline remains subject to architecture review. No app UI,
camera, receiver or hardware acceptance test was run.
