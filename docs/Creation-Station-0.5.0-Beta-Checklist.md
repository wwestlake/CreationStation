# Creation Station 0.5.0 Beta Checklist

This is the working definition of done for the first real beta-quality Creation Station release.

## Release Goal

Creation Station 0.5.0 beta should feel like a usable creative audio workstation: projects save cleanly, templates work, audio recording/playback is reliable, clips can be edited, generated sounds can be reused, and the app can export useful audio.

## Checklist

### Project System

- [x] Save `.csp` project packages with manifest metadata, session state, tracks, clips, assets, renders, DSL, and undo state.
- [x] Open `.csp` project packages without losing clip paths or project assets.
- [x] Save `.cst` templates from configured projects.
- [x] Create new projects from `.cst` templates with assets copied into the new project.
- [x] Add Save As / New / Open flow that feels normal and predictable.
- [x] Show a dirty-project prompt before closing or replacing unsaved work.
- [ ] Keep workspace layout files outside project packages.

### Tracker Editing

- [x] Select tracks and clips clearly.
- [x] Move clips across time.
- [x] Move clips between compatible tracks.
- [x] Split clips at the playhead.
- [x] Delete, duplicate, and rename clips.
- [x] Add undo/redo for timeline edits.
- [x] Keep right-click menus anchored at the mouse click.
- [x] Add useful hotkeys for common edits.

### Studio I/O

- [ ] Finish studio-grade input/output setup.
- [ ] Support ASIO4ALL, FlexASIO, Windows Audio, and Windows Low Latency where available.
- [ ] Split stereo hardware inputs into named mono studio inputs.
- [ ] Persist studio input names and selected audio settings.
- [ ] Show driver diagnostics and open the driver panel when available.
- [ ] Record multiple armed tracks from separate named inputs.
- [ ] Show calibrated dB meters with green/yellow/red ranges.
- [ ] Support per-track monitoring, mute, solo, volume, master volume, and stereo tracks.

See `docs/Studio-Grade-Audio-Routing-Checklist.md` for the detailed audio routing checklist.

### Signal And Foley

- [ ] Save named Signal Lab designs inside the project.
- [ ] Render Signal Lab sounds to reusable project assets.
- [ ] Audition Signal Lab sounds inside the tool.
- [ ] Save named Foley designs inside the project.
- [ ] Build Foley from layered reusable sounds.
- [ ] Add Foley variation controls for repeated sounds like footsteps.
- [ ] Place generated Signal/Foley assets onto tracker tracks.

### Mixer And Effects

- [x] Route tracker playback through track mixer controls.
- [ ] Make mixer faders affect playback and render.
- [ ] Make pan affect playback and render.
- [x] Make mute/solo consistent across tracker, mixer, and X-Touch.
- [ ] Support track FX chains.
- [x] Support single track insert FX.
- [x] Support master insert FX.
- [ ] Keep third-party VST UI open/close behavior reliable.

### Render And Export

- [x] Render full project to WAV.
- [ ] Render selected time range to WAV.
- [ ] Render selected tracks or stems.
- [ ] Add peak normalization.
- [ ] Add RMS normalization.
- [x] Keep render output inside the project unless the user exports externally.
- [x] Export finished audio to a user-selected file.
- [x] Export project WAV/render assets unchanged in raw form.

### Control Surfaces

- [ ] Keep X-Touch transport, faders, bank switching, mute, solo, record-arm, and master fader reliable.
- [ ] Stabilize X-Touch scribble strip updates or deliberately limit to the stable top-line behavior.
- [ ] Support BCR2000 mapping profiles.
- [ ] Add a visual control-surface mapping view.
- [ ] Allow user-customizable control labels and assignments.

### Virtual Engineer

- [ ] Keep the Virtual Engineer visible as a right-side assistant panel.
- [ ] Support OpenAI BYOK.
- [ ] Support Ollama local provider selection.
- [ ] Load model lists from providers where possible.
- [ ] Route app context through LiteSemRAG when available.
- [ ] Add permission levels for safe app actions.
- [ ] Begin exposing app action handles so the assistant can operate the workstation.

## Beta Test Standard

Each completed slice needs a short manual test:

- What to click.
- What should visibly change.
- What sound should be recorded, played, rendered, or muted.

0.5.0 beta is ready when the checklist is mostly green and the remaining gaps are clearly marked as known beta limitations.
