# Minimal Viable Cable

This is the first real audio-cable target for Djehuti Router.

## Cable shape

- render endpoint: `Djehuti Router Monitor`
- capture endpoint: `Djehuti Router Capture`
- sample rate: `48 kHz`
- channels: `2`
- target latency: `10 ms`

## Behavior

- apps send audio into the render side
- the driver stores that audio in a shared buffer
- the capture side reads from the same buffer
- no EQ, effects, or mixing yet
- no extra mic or instrument paths yet

## Why this is the right first step

This gives us the smallest possible Windows-visible cable that Reaper and OBS can learn to use before we add anything fancy.

