# Creation Suite Interop Specification

This document defines the shared interop contract for the two primary
Creation apps in active integration work today:

- Creation Station
- Creation Engine

Creation Movie and Creation Live should adopt this same contract after
the Station/Engine path is proven stable.

The goal is seamless transfer of assets, products, and CEL code between
apps without path fragility, silent mutation, or per-app private formats
drifting apart.

## Core Principles

1. One asset may be used by many apps.
2. Asset identity must be stable across time.
3. Exact asset versions must be immutable.
4. Projects may reference shared assets without copying them.
5. Projects may optionally embed or vendor assets for portability.
6. CEL code with no domain-specific calls must compile in every app.
7. Domain-specific CEL must be explicitly gated by host policy.

## Two Storage Layers

The suite uses two related but distinct storage layers.

### 1. Suite Asset Store

The suite asset store is the canonical shared resource system for:

- audio
- midi
- CEL source
- generated CEL graphs
- presets
- textures
- meshes
- scenes
- renders
- other reusable content

The suite asset store owns stable asset identity and immutable versions.

### 2. Project VFS

Each app project remains its own project-local VFS/document container.

The project VFS stores:

- arrangement/session state
- project metadata
- local documents
- local overrides
- references to suite assets
- optional vendored copies of referenced assets for portability

The project VFS is not the canonical shared store.

## Asset Identity Model

Every shared resource is described by two identities:

- `asset_id`: stable identity for the conceptual item
- `version_id`: stable identity for one exact immutable revision

Meaning:

- `asset_id` answers "what thing is this?"
- `version_id` answers "which exact bytes and metadata revision is this?"

If content changes, the suite creates a new `version_id`.

It does not mutate an existing version in place.

## Reference Modes

Apps and projects may reference shared assets in three modes.

### Exact

Pin to one exact immutable version.

Use for:

- reproducible playback
- deterministic builds
- safe archival
- frozen project delivery

### Compatible Latest

Pin to an asset identity plus a compatibility rule, then resolve to the
newest acceptable version.

Use for:

- rolling library updates inside a stable content family

### Latest

Always resolve the newest available version for an asset identity.

Use for:

- live libraries
- exploratory workflows
- intentionally floating references

## Asset Record Fields

Each canonical asset version record should contain at minimum:

- `asset_id`
- `version_id`
- `logical_name`
- `kind`
- `media_type`
- `content_hash`
- `created_at`
- `created_by_app`
- `source_tool`
- `domain_tags`
- `dependency_refs`
- `metadata`

Recommended `metadata` examples:

- audio: sample rate, channels, bit depth, duration, peak, loudness
- midi: track count, tempo hints, duration
- CEL: declared domains, exports, ABI/runtime version
- graph: source language, generated CEL checksum, node catalog version

## Content Addressing

Every immutable asset version should also carry a strong content hash.

The content hash is not the user-facing identity, but it is used for:

- integrity verification
- deduplication
- caching
- reproducible dependency resolution

Two different `asset_id` records may point at identical bytes, but a
single `version_id` must always map to one exact content hash.

## Project Asset References

Projects should stop relying on raw filesystem paths as their primary
asset links.

A project reference should instead contain:

- `asset_id`
- reference mode
- pinned `version_id` when exact
- optional fallback vendored path inside the project VFS

This means a tracker clip, patch, or scene item should resolve by asset
reference first, not by absolute or loose relative OS file path.

## Vendoring and Portability

Projects may optionally vendor suite assets into the project VFS.

Vendoring is allowed for:

- portable exports
- air-gapped transfer
- archival snapshots
- publication bundles

When vendored, the project should still preserve the original shared
asset reference in metadata so provenance is not lost.

## Shared CEL Rules

CEL is the shared procedural language across the Creation suite.

There are two categories of CEL:

### Core CEL

Core CEL contains only:

- language syntax
- control flow
- generic math
- generic data transforms
- non-domain-specific utility calls

Core CEL must compile in every Creation app.

### Host CEL

Host CEL uses app-specific intrinsic domains such as:

- `world`
- `audio`
- `patch`
- `tracker`
- `mixer`
- `performance`
- later Movie/Live domains

Host CEL may compile only in apps whose policy explicitly allows those
domains.

## CEL Asset Manifest

Every shared CEL asset should carry a manifest describing:

- `asset_id`
- `version_id`
- `entry_points`
- `required_domains`
- `exported_symbols`
- `dependency_refs`
- `abi_version`
- `language_version`
- `generated_from`

`generated_from` may point to:

- user-authored `.cel`
- node graph asset
- future higher-level authoring tools

## Node Graph Relationship To CEL

Visual node graphs are not a separate final scripting language.

The suite standard is:

- node graph is an authoring format
- CEL is the executable language
- graph generation must target CEL
- diagnostics should map back to graph nodes where possible

This keeps one executable language family across apps instead of one
language per tool.

## Versioning Rules

The suite follows immutable versioning.

Rules:

1. `version_id` never changes in place.
2. Byte changes create a new version.
3. Metadata changes that affect behavior create a new version.
4. Stable conceptual grouping stays under the same `asset_id`.
5. References choose exact, compatible-latest, or latest behavior.

## Shared Dependency Rules

Any asset may depend on other assets by shared references.

Examples:

- a Station patch graph may depend on wave assets and CEL helpers
- an Engine scene may depend on mesh, texture, and CEL helper assets
- a shared CEL library may depend only on other CEL libraries

Dependencies must be recorded as references to exact versions or to a
declared resolution policy.

## Inter-App Compatibility

An app may open or consume an asset only if:

1. the asset kind is recognized
2. the required domains are allowed by host policy
3. the referenced version resolves
4. the runtime/ABI requirements are satisfied

If not, the app must fail clearly and non-destructively.

## Initial Implementation Direction

For the first integration pass:

1. Creation Engine's CEL implementation is the reference language core.
2. Creation Station replaces its current private language path with the
   shared CEL path.
3. Creation Station and Creation Engine converge on shared asset
   references before Movie/Live begin deeper implementation.
4. The existing Engine zip-backed VFS work is the reference direction
   for project-local virtual storage.
5. Station's current project asset model should be migrated toward
   shared `asset_id` and immutable `version_id` semantics.

## Immediate Next Steps

1. Define the shared asset reference structure in code.
2. Add exact-version asset references to Creation Station project data.
3. Add capability manifests for CEL assets.
4. Expand CEL domain gating beyond `core` and `world` as Station audio
   intrinsics come online.
5. Introduce a suite asset catalog/index layer above per-app project
   storage.

## Non-Goals For This Phase

This document does not require:

- one giant monorepo
- one shared runtime process
- immediate cross-app live editing
- immediate networked asset serving
- immediate Movie/Live implementation work

The target for this phase is one stable interop contract that Station
and Engine can both implement cleanly first.
