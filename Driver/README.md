# Djehuti Router Driver Scaffold

This folder is the starting point for the Windows virtual audio driver that will expose the router endpoints to OBS, Reaper, and other apps.

## Why it exists

JUCE can run the router UI and internal audio flow, but it cannot create new Windows audio devices by itself.  
The actual named endpoints will come from a Windows driver based on Microsoft's SysVAD sample.

## Target endpoints

- `Djehuti Router Capture`
- `Djehuti Router Monitor`
- `Djehuti Router Mic`
- `Djehuti Router Instrument`

## First constraints

- sample rate: `48000 Hz`
- initial block size target: `512`
- keep the device names stable
- make the endpoints readable in OBS and Reaper

## Reference material

- `wiki/Virtual-Driver-Path.md`
- Microsoft SysVAD sample
- Microsoft audio driver sample docs

## Folder map

- `Driver/Audio` for the SysVAD audio-driver plan and first cable layout
- `Driver/Contract` for the shared device names and timing contract
- `Driver/INF` for the future Windows install descriptor
- `Driver/Project` for the WDK solution and project skeleton
- `Driver/Source` for the actual WDK driver code when we add it
- `Driver/Build` for build notes and WDK-specific steps
- `Driver/Package` for driver installer packaging notes

## What gets added later

- WDK project files
- INF packaging
- endpoint registration
- render/capture pin wiring
- validation and install steps
