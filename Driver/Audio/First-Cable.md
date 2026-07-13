# First Cable

The first real driver milestone should be a single working virtual audio cable.

## What it needs

- a render side that Windows apps can send audio to
- a capture side that other apps can read from
- a matching install package and INF
- names that line up with the app contract
- a fixed `48 kHz`, `2 channel` format for the first pass
- a shared buffer that moves audio from render to capture

## Suggested first names

- `Djehuti Router Monitor`
- `Djehuti Router Capture`

## Why this is the right first slice

SysVAD is designed to show how to expose multiple audio devices in a Windows driver. Starting with one cable keeps the work small enough to debug.
