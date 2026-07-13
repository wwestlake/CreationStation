# Versioning

This repo ships three separate products from one source tree, and each product gets its own release tag prefix.

## Products

- `creation-station`
- `djehuti-router`
- `djehuti-drivers`

## Tag rule

Use the product prefix first, then the release version:

- `creation-station-v0.1.2`
- `djehuti-router-v0.1.2`
- `djehuti-drivers-v0.1.2`

The server uses the literal start of the tag to decide which product gets the release.

## Shared version rule

If we want the three products to move in lockstep, we can give them the same version number and publish the three tags close together:

- `creation-station-v0.1.2`
- `djehuti-router-v0.1.2`
- `djehuti-drivers-v0.1.2`

That keeps the family easy to reason about while still letting each product ship as its own artifact.

## Release rule

- Each product release tag publishes only that product's assets.
- It is fine for one product to move ahead internally while the others are still catching up.
- The repo can keep working builds before the first public release lands.
- The tag controls the product name, and the version stays in the tag itself.
