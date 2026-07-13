# SysVAD Path

Microsoft's SysVAD sample is the best starting point for the audio driver we want.

## Why SysVAD

- it demonstrates the Windows audio driver architecture
- it supports multiple audio devices
- it gives us a known-good layout for render/capture and later extensions

## Our adaptation

- keep the app-side cable model as the naming source of truth
- map the first cable into a Windows-visible endpoint
- add extra endpoints only after the first cable is stable

## Reference links

- [Audio driver samples](https://learn.microsoft.com/en-us/windows-hardware/drivers/samples/audio-driver-samples)
- [SysVAD sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/)
- [Sample audio drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/sample-audio-drivers)

