# SysVAD Handoff

This is the handoff from the current driver shell to the real Windows audio driver shape.

## What Microsoft gives us

- the SysVAD sample as the reference audio-driver architecture
- sample audio driver guidance for WDM audio work
- a known place to model render and capture endpoints

## What we do next

1. copy the SysVAD project layout into the driver workspace
2. map the first cable to the shared endpoint contract
3. keep the names stable while the audio plumbing is built
4. add the INF/install package for the virtual device
5. wire the first cable into Windows so apps can see it
6. follow `Driver/Audio/SysVAD-Integration-Plan.md` for the file-by-file split

## Why we do it this way

Starting with the sample layout keeps us much closer to a working Windows audio device than trying to invent the plumbing from scratch.
