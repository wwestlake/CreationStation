# SysVAD Integration Plan

This is the first real driver plan for turning the current shell into a working virtual audio cable.

Microsoft’s SysVAD sample is the right base because it already shows a WDM virtual audio device with render and capture behavior.

## Goal for the first pass

- one stereo cable
- one render endpoint: `Djehuti Router Monitor`
- one capture endpoint: `Djehuti Router Capture`
- fixed `48 kHz`
- simple buffering only
- no EQ, no effects, no mixer, no extra endpoints

## What we will add to the repo

### Driver sample layout

Copy the SysVAD sample structure into the driver workspace as the real audio-driver base.

Suggested home:

- `Driver/SysVAD/`

Current scaffold:

- `Driver/SysVAD/README.md`
- `Driver/SysVAD/Adapter/`
- `Driver/SysVAD/Miniport/`
- `Driver/SysVAD/Topology/`
- `Driver/SysVAD/Streaming/`
- `Driver/SysVAD/Buffer/`
- `Driver/SysVAD/Install/`

### Core driver pieces

- adapter and miniport plumbing
- pin and endpoint setup for the first cable
- shared buffer logic for render-to-capture transfer
- INF and install metadata for the new virtual device name
- package step for the driver-only installer

### Contract files already in place

- `Driver/Audio/DjehutiAudioCableContract.h`
- `Driver/Audio/MVP-Cable.md`
- `Driver/Audio/EndpointMap.md`

## Suggested file split after import

- `Driver/SysVAD/Adapter/`
- `Driver/SysVAD/Miniport/`
- `Driver/SysVAD/Topology/`
- `Driver/SysVAD/Streaming/`
- `Driver/SysVAD/Buffer/`
- `Driver/SysVAD/Install/`

## First implementation order

1. copy the SysVAD sample into `Driver/SysVAD`
2. rename the exposed device to the Djehuti Router cable names
3. wire the first render and capture endpoints
4. keep the sample format locked to `48 kHz`, stereo
5. connect the shared buffer between render and capture
6. package the driver for install

## What this does not try to do yet

- no multi-cable routing matrix
- no DSP chain
- no APO effects
- no mic/instrument split beyond the first cable
- no fancy UI linkage yet

## Success check

The first milestone is simple: install the driver, open Reaper or OBS, and see a real `Djehuti Router` audio device that can pass audio through the first cable.
