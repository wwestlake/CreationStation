# Creation Station AI Context Architecture

## Purpose

Creation Station should not treat AI context as a plain chat transcript. It should treat context as a **local semantic operating space** assembled from project state, language artifacts, patch structures, content library assets, and recent user intent. The AI layer should retrieve from this space dynamically and respond to sharp thematic pivots without dragging stale context forward.

This design adapts the **LiteSemRAG + Information Space Dynamics (ISD)** model into the Creation Station desktop client.

## Core Idea

The architecture combines two layers:

- **LiteSemRAG layer**: local semantic retrieval over project-relevant documents and artifacts
- **ISD layer**: motion analysis over the user's evolving prompt trajectory

LiteSemRAG gives us fast local recall. ISD tells us when the user is staying on-theme, drifting, pivoting hard, or getting trapped in a stale attractor basin.

## Context Sources

The AI subsystem should ingest and index:

- Patina source buffers
- Patina artifact JSON files
- Signal Lab patches
- project manifests and paths
- track, channel, and routing metadata
- content-library summaries and installed assets
- session state, workspace mode, and recent decisions
- selected future sources such as plugin parameter snapshots and node graph state

Each source becomes a local **context document** with category, tags, body text, source path, and update timestamp.

## Thread Boundary

The JUCE message thread must remain clean. Context assembly runs in a dedicated background worker.

- **UI thread**
  - captures user prompt
  - publishes current app state as source documents
  - displays returned context packet
- **context worker**
  - accepts retrieval requests
  - tokenizes and ranks source documents
  - computes ISD dynamics
  - builds a context packet
  - posts the result back asynchronously

This preserves UI responsiveness and gives us a clean upgrade path to real embedding-based retrieval later.

## ISD Metrics For Creation Station

The following metrics are adapted from the ISD model into creative workstation behavior:

- **semantic velocity**
  - how different the current prompt is from the previous prompt
- **reference drift**
  - how far the current prompt has moved from the session anchor
- **curvature**
  - how sharply the user pivoted between prompts
- **torsional resistance**
  - a synthetic attractor-risk value derived from drift and curvature
- **recovery suggestion**
  - raised when the system should broaden retrieval and down-rank stale context

These metrics allow the assistant to avoid sticky behavior and recover gracefully when the user abruptly changes direction.

## Retrieval Packet

The output of the engine is a **context packet** containing:

- request metadata
- ISD dynamics state
- ranked local snippets
- a compact human-readable summary

This packet is what the AI panel should ultimately feed into:

- BYOK LLM prompt construction
- local patch drafting
- Patina synthesis requests
- future LiteSemRAG + ISD trace logging

## Current Scaffold

The current codebase now contains a first native scaffold:

- `Source/AI/CreationStationContextEngine.h`
- `Source/AI/CreationStationContextEngine.cpp`

This first version provides:

- in-memory local source document registry
- background request worker
- token-overlap retrieval
- ISD-style prompt motion metrics
- ranked context packet assembly
- AI panel integration

This is intentionally lightweight and dependency-free so it can live cleanly inside the current JUCE app.

## Planned Evolution

### Phase 1

- prompt-based context packet assembly
- local session/project source ingestion
- AI panel preview of ranked snippets and ISD metrics

### Phase 2

- Patina artifact ingestion
- signal graph, patch, and content metadata ingestion
- persistent local context store

### Phase 3

- real vector embeddings through BYOK provider
- LiteSemRAG clustering and local semantic neighborhoods
- USearch-backed ANN retrieval

### Phase 4

- full ISD-guided retrieval modulation
- recovery routing when the session crosses a phase boundary
- fine-grained context policies per workspace mode

## Design Rule

The assistant must never blindly stuff all recent state into the prompt. It should always assemble **targeted, motion-aware context**.

That is the real value of the LiteSemRAG + ISD fusion inside Creation Station.
