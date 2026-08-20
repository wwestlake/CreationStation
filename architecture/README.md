# Creation Station Architecture Model

This folder is the machine-readable and human-readable architecture map for Creation Station.

Use it as the source of truth for:
- major app parts and how they connect
- audio flow, control surface flow, DSL flow, and content flow
- release and deployment topology
- future diagram generation for wiki pages or review sessions

## Layout

- `model/` — structured project model files
- `diagrams/` — generated Mermaid diagrams for quick review
- `decisions/` — architecture decisions and tradeoffs

## How to read it

- Start with `model/system.yaml`
- Then read `model/components.yaml`
- Then open the Mermaid diagrams in `diagrams/`

## Maintenance rule

Keep this folder aligned with the codebase.
When the app changes, update the model files first or alongside the code.
