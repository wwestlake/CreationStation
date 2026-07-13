# Driver Source Scaffold

This folder will hold the actual Windows audio driver code.

## What belongs here later

- the WDK driver entry point
- SysVAD-based device plumbing
- endpoint registration for the named virtual cables
- capture and render path wiring
- the timing and buffer handling needed for OBS and Reaper

## First goal

Ship one stable virtual cable first, then grow the rest of the endpoints after the base device is reliable.
