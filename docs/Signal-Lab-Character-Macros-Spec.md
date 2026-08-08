# Signal Lab Character Macros Spec

Date: August 5, 2026
Status: Draft
Scope: Creation Station / Signal Lab

## Purpose

Signal Lab is the brick maker for Creation Station.

Its job is to create sound material from signal: hits, tones, textures, swells, impacts, drones, and other reusable sonic bricks. The Character Macros are the high-level shaping controls that let a user move quickly through sonic territory without manually tuning every low-level parameter.

These macros are not decorative UI. They are first-class sound-design controls.

## Product intent

The Character Macros must:

- change the character of a generated sound in musically meaningful ways
- feel broad and expressive rather than technical and fiddly
- work across short and long generated sounds
- be automatable inside Signal Lab
- be saved as part of the editable sound object
- affect rendering deterministically

The Character Macros must not:

- turn Signal Lab into Foley
- load or browse WAV files
- behave like tracker/timeline automation for arrangement
- expose implementation details to the user as the primary interaction model

## The five macros

### 1. Hardness

Hardness controls how sharp, firm, and immediate a sound feels.

At lower values, sounds should feel softer, rounder, and more padded.
At higher values, sounds should feel more percussive, click-forward, edge-defined, and assertive.

Hardness may influence:

- attack speed
- early envelope slope
- transient emphasis
- high-mid presence
- filter envelope bite
- square/bright harmonic contribution

Hardness must not primarily mean distortion. That belongs more to Grit.

### 2. Weight

Weight controls how heavy, grounded, and mass-rich a sound feels.

At lower values, sounds should feel lighter, narrower, and less physically substantial.
At higher values, sounds should feel fuller, denser, lower-centered, and more anchored.

Weight may influence:

- low-mid emphasis
- body/sustain amount
- envelope body level
- fundamental dominance
- perceived mass
- downward bias in brightness or pitch behavior

Weight must not simply mean “louder” or “more bass.” It is about perceived mass, not a crude gain boost.

### 3. Air

Air controls openness, breath, shimmer, and spatial lightness.

At lower values, sounds should feel closed, dry, and contained.
At higher values, sounds should feel more open, breathy, lifted, and lightly extended upward.

Air may influence:

- upper-frequency openness
- noise brightness contribution
- spectral lift
- sense of space and openness
- reduced congestion in the middle

Air must not automatically mean reverb. Space can be implied spectrally without forcing a room effect.

### 4. Grit

Grit controls roughness, texture, saturation, abrasion, and instability.

At lower values, sounds should feel smoother and cleaner.
At higher values, sounds should feel more scraped, driven, textured, noisy, or worn.

Grit may influence:

- waveshaping / saturation
- rough harmonic density
- noise contribution
- unstable or ragged edge behavior
- saw/noise prominence

Grit is the macro most allowed to get ugly. It should be able to produce character, dirt, scrape, and damage when pushed.

### 5. Size

Size controls perceived scale and temporal/spatial footprint.

At lower values, sounds should feel smaller, tighter, nearer, and more compact.
At higher values, sounds should feel larger, broader, slower, and more expansive.

Size may influence:

- envelope length and spread
- sustain/release feeling
- temporal bloom
- spectral spread
- perceived physical scale

Size must not require the sound itself to become a timeline event. It is still one generated object, just with a different sense of scale.

## Interaction model

The user should understand these as adjective controls, not engineering controls.

The intended mental model is:

- Hardness = softer ↔ sharper
- Weight = lighter ↔ heavier
- Air = closed ↔ open
- Grit = clean ↔ rough
- Size = small ↔ large

They should be presented as normalized 0..1 controls internally, but the user-facing experience should emphasize character over numbers.

## Implementation rule

Each macro should drive multiple lower-level behaviors at once.

A macro is successful when:

- moving it clearly changes the sound’s identity
- the result feels coherent
- the user does not need to know what internal parameters were moved

A macro is unsuccessful when:

- it behaves like a single hidden knob
- the sonic change is too subtle
- it overlaps so heavily with another macro that users cannot tell them apart

## Recommended parameter mapping direction

These are target behaviors, not a final DSP mandate.

### Hardness target mapping

- faster attack contour
- stronger transient weighting
- brighter filter-envelope push
- more square/edge harmonic emphasis
- reduced softness in the initial body

### Weight target mapping

- stronger fundamental/body emphasis
- fuller sustain contour
- slightly lower spectral center of gravity
- increased low-mid density
- greater perceived mass

### Air target mapping

- increased high-frequency openness
- brighter, finer noise layer behavior
- less mid congestion
- lifted spectral tail feel

### Grit target mapping

- stronger saturation/waveshaping
- increased rough harmonics
- increased abrasive noise texture
- less polished edge behavior

### Size target mapping

- longer bloom / spread behavior
- wider envelope staging
- larger-feeling temporal shape
- less “tiny object” immediacy

## Guardrails

The five macros should be distinguishable.

Guardrails:

- Hardness and Grit must not collapse into the same thing
- Weight and Size must not collapse into the same thing
- Air must not merely duplicate filter cutoff
- macro extremes must remain renderable and intentional
- automation of macros must not destabilize patch reconstruction

## Automation and persistence

The Character Macros are core Signal Lab parameters and must:

- be targetable by Signal Lab internal automation
- support curve-based automation over time
- be saved inside the editable sound object
- participate in undo/redo
- round-trip through project asset save/load
- reproduce consistently when rendered to WAV

## Non-goals

This macro system does not define:

- WAV import into Signal Lab
- Foley workflows
- sampler note-mapping workflows
- tracker/timeline arrangement automation
- external plugin parameter automation

Those belong to other tools.

## Relationship to the current prototype

The current implementation already contains the five macros and uses them in early shaping logic. That implementation should be treated as a prototype interpretation, not the final spec.

Future work should align the code to this document, not treat the current behavior as authoritative merely because it exists first.
