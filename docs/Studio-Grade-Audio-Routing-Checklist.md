# Studio-Grade Audio Routing Checklist

This is the build tracker for making Creation Station behave like a real studio, not a demo toy.

## Goal

A track records from a named studio input, not from a vague device slot. The user configures hardware once in Settings, names the sources clearly, then tracks choose from those studio names.

## Current Baseline

- Audio systems enumerate through JUCE, including Windows Audio modes and ASIO when the ASIO SDK build support is present.
- Settings can select audio system, input device, and output device.
- Active input channels become named Studio Inputs.
- Stereo inputs can become separate mono sources when the selected driver reports both channels.
- Track headers can choose a Studio Input, arm, monitor, record, and show an input meter.
- Recording writes one mono WAV per armed track.

## Work Items

### 1. Studio Input Naming
Status: in progress

- Settings shows hardware details.
- Tracker shows only friendly studio names.
- Names persist in app settings.
- Missing channels remain visible as missing instead of silently changing meaning.

### 2. Driver and Channel Diagnostics
Status: in progress

- Show selected driver, sample rate, buffer size, active input channel count, and input channel names.
- Make it obvious when Windows reports a stereo device as mono.
- Make ASIO4ALL/FlexASIO easier to verify from inside the app.
- Show an Open Driver Panel button only when the active driver exposes one.

### 3. Per-Track Source Assignment
Status: in progress

- Tracks select named Studio Inputs.
- Project stores track intent safely.
- The audio engine resolves that intent to the current hardware channel.
- Missing inputs should disable recording for that track and show a clear warning.

### 4. Multi-Track Recording
Status: partially working

- Multiple armed tracks can write separate WAV files.
- Each armed track must record its assigned Studio Input.
- Takes need clean placement on the timeline after stop.
- Removing a track must remove its clips and recording state, not resurrect stale data.

### 5. Monitoring and Meters
Status: partially working

- Armed/monitored tracks show live input meters.
- Monitor path routes selected input to output with track gain/pan.
- Need dB meter calibration and clip indication.

### 6. Studio I/O Profiles
Status: pending

- Save named I/O profiles outside project files.
- Support common setups: headset, UMC22 split mono, ASIO4ALL, FlexASIO, Voicemeeter/Cable A/B.
- Projects should not force machine-specific audio settings.

### 7. Render and Import Integration
Status: pending

- Timeline clips, generated Foley, Signal Lab output, VST output, and recordings become project assets.
- Render can output WAV/FLAC/MP3 later.
- Normalization options: peak, true peak if available, RMS/LUFS later.

## Testing Rule

Each slice gets one simple user test: what to click, what should visibly happen, and what audio should be heard or recorded.
