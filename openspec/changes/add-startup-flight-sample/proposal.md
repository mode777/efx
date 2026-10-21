# Proposal: add-startup-flight-sample

## Why

The gallery has no sample that reads as a product identity moment: every
showcase demonstrates a feature, none presents EFX itself. A startup-screen
scene — a flight along the edge of a cloud deck under a clear blue sky with
the EFX logo swaying gently in the center — doubles as a showcase of the
procedural noise API (ADR 0059) and the 2D painter's-ordered layer, and is
built so it can later be promoted to the player's actual boot screen
(promotion itself is out of scope).

## What Changes

- Add a new curated gallery sample `gallery/samples/curated/startup-flight/`
  (single `main.js` plus a copy of the repo-owned `assets/efx_logo.png`;
  every other asset is generated procedurally at load).
- The scene is 100% 2D in one painter's-ordered record sequence:
  sky gradient + haze + sun rays → cloud billows drawn far-to-near in one
  sprite batch → lens flare → logo effects, finished by a bloom pass.
- Cloud billows are generated at boot with engine means only: smooth-
  unioned sphere-cap height fields with `noise.fill2`-wobbled rims, lit
  per-pixel in JS (sun-lit normals, crest-to-base blue-grey gradient) into
  one atlas texture (`createImageData` → `createTexture`, clamped,
  mipmapped).
- Perspective is computed in JS (screen pos + size from `f·x/z`); clouds
  stream straight toward the camera, recycler-style, tint-lerped toward the
  horizon haze color with distance, drawn back-to-front. No 3D camera, no
  billboards, no render targets in the frame path.
- The logo floats in the sky above the deck with a pop-in, flash, shine
  sweep and glints; a sun with god rays and a lens flare, and a bloom post
  chain, make the boot moment flashy (design D7).
- Register the sample in `gallery/samples/curated/manifest.json` and add a
  gallery smoke test in `tests/CMakeLists.txt`.

`skip_specs` is set (`.openspec.yaml`): no script-facing API or engine
behavior changes — this is a sample authored entirely on the public API,
so no capability spec is created or modified.

## Capabilities

### New Capabilities

- None (`skip_specs: true` — no spec-level behavior changes).

### Modified Capabilities

- None.

## Impact

- **New files**: `gallery/samples/curated/startup-flight/main.js`,
  `gallery/samples/curated/startup-flight/efx_logo.png` (copy of
  `assets/efx_logo.png`).
- **Modified**: `gallery/samples/curated/manifest.json` (catalog entry,
  category "Showcase"), `tests/CMakeLists.txt` (one
  `smoke_showcase_startup_flight` player test).
- **Docs**: No ADR — sample-only change, no durable architecture decision;
  no `js-api.md` or `docs/api/` changes (zero API surface change); no
  `CREDITS.md` change (logo is repo-owned, all other art procedural).

## Non-goals

- Wiring the sample into the player as the no-args boot screen (future
  change; would touch `src/player`, vision.md, and the player-runtime spec).
- A golden-image baseline for the scene (the scene is deterministic and is
  a strong future golden candidate, but new goldens need the server-side
  llvmpipe capture recipe; not required for a sample-only change).
- Fly-through beat (screen washing white when a puff crosses the camera) —
  billows tall enough to reach eye level only spawn clear of the flight lane.
- Audio or input reactions.
