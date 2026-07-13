# Release Products

This repo ships three separate products from one source tree.

## Products and slugs

- `creation-station` → `CreativeWorkstation`
- `djehuti-router` → `DjehutiRouter`
- `djehuti-drivers` → `DjehutiRouterDriver`

## Tag prefixes

Use a product-prefixed tag so the server can route the release correctly:

- `creation-station-v0.1.2`
- `djehuti-router-v0.1.2`
- `djehuti-drivers-v0.1.2`

## Release rule

Each product tag should publish only that product's assets. If we want to move all three together, we can tag all three with the same version number in a lockstep release run.

For the version policy, see `Versioning.md`.
