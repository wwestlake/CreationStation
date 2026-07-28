# Creation Suite Asset System Standard

This document records the suite-wide storage direction shared by Creation Station, Creation Engine,
Creation Movie, and Creation Live.

## Standard

- The suite has one system-level VFS location that every app can find.
- That suite VFS stores shared configuration, discovery metadata, and shared resources used across
  the whole product family.
- Each app uses the suite VFS to locate its own project roots and, when needed, other apps'
  project roots.
- A project archive is the canonical virtual filesystem container inside an app's project space.
- Asset identity is shared across the suite through `AssetId`, `AssetVersionId`, and `AssetRef`.
- Reads happen through a shared zip-backed virtual filesystem library, not by ad hoc direct file
  path assumptions inside each tool.
- Tools may materialize a temporary real file only when a dependency truly requires a filesystem
  path, such as some third-party plugins.

## Shared Library

The suite-level core lives in the `creation_asset_system` library.

Current contents:

- `creation::assets::VirtualFileSystem`
- shared asset identity/value types in `creation::assets::AssetTypes`

Future shared additions:

- suite VFS locator/bootstrap metadata
- shared project registry/discovery records
- common materialization rules for temp-file-only integrations

## Adoption Plan

1. Add the suite VFS bootstrap/discovery layer so every app resolves shared config and project
   roots the same way.
2. Creation Station migrates its project storage layer to use the shared asset types and VFS.
3. Foley and other file-oriented tools stop storing raw asset filenames and store `AssetRef`
   instead.
4. Creation Engine adopts the same shared type definitions so both applications speak the same
   asset language.
5. Creation Movie and Creation Live build on the same library from the start.
