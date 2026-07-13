# SysVAD Scaffold

This folder is the landing zone for the real Windows audio driver port.

## Layout

- `Adapter/` for device and adapter setup
- `Miniport/` for endpoint miniports
- `Topology/` for the control graph
- `Streaming/` for render/capture flow
- `Buffer/` for the shared cable buffer
- `Install/` for INF and package metadata

## First target

Build one stereo cable first:

- `Djehuti Router Monitor`
- `Djehuti Router Capture`

The first pass stays fixed at `48 kHz` and uses one shared buffer between render and capture.

