# Audio Driver Plan

This folder is the bridge from the generic KMDF driver skeleton to a real virtual audio device.

## Microsoft sample path

Microsoft points custom virtual audio driver work at the **SysVAD** sample, which shows the WDM audio architecture and supports multiple audio devices.  
Sources:
- [Audio driver samples](https://learn.microsoft.com/en-us/windows-hardware/drivers/samples/audio-driver-samples)
- [SysVAD sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/)

Our repo scaffold for that work starts under `Driver/SysVAD/`.

## First working goal

Build one cable first:

- one render endpoint for playback/monitoring
- one capture endpoint for input back into apps
- stable names that match the app-side contract
- shared contract lives in `Driver/Audio/DjehutiAudioCableContract.h`
- the concrete minimum cable spec lives in `Driver/Audio/MVP-Cable.md`

## Later expansion

- extra named routes for mic and instrument
- clearer routing presets from Djehuti Router
- optional APO effects and cable processing later

## Next build step

- `Driver/Audio/SysVAD-Handoff.md`
- `Driver/Audio/SysVAD-Integration-Plan.md`
- copy the SysVAD layout into the workspace
- wire the first cable to the shared endpoint contract
- keep the first pass fixed at 48 kHz stereo with a simple shared buffer
