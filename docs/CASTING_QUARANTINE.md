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

A complete-history bundle, verified by `git bundle verify`, is retained in the
ignored canonical build directory as
`build/station-casting-preservation-20261118.bundle`. The preservation branch and
bundle are local; the mixed branch has not been published as a shipping change.
Do not discard these records during build-folder cleanup.

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
