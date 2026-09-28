# Design

## Context

F10 (CommonJS modules) is implemented: `require` resolves synchronously from
the dir/zip resource root through the shared pure-JS runtime, with caching,
cycles, `__esModule` interop, and JSON modules (ADR 0037). The gallery hosts
each sample in an isolated iframe, feeds the entry source over the
`__efx_main_js` channel, and mounts at most one asset zip as the resource root
(ADR 0030, F6a). This change adds one curated sample and one authored asset
pack; it touches no engine, shell, or runner code.

## Goals / Non-Goals

**Goals:** a single sample that (a) makes the split-across-files model
visible, (b) demonstrates relative resolution, the deterministic `.js`
fallback, and a JSON module, (c) stays a legible, lit 3D scene, and (d) is
robust in the headless smoke (small fixed work per frame, no RNG).

**Non-Goals:** new API, engine/renderer change, golden scene, gallery UI
change, third-party assets.

## Decisions

**D1 — The dependency graph is shown, not hidden.** The entry begins with a
short `//` block that names each required file and what it contributes, so a
visitor reading the editor sees the module boundaries even though the module
sources live in the pack. `require` is used with a relative path, an
extension-less path, and a `.json` path.

**D2 — Authored modules ship as a committed pack plus readable sources.**
The embedding contract mounts one zip as the resource root, so the required
files must be inside a pack. To keep them reviewable, the sources are
committed under `gallery/samples/curated/modules/` and packed deterministically
by `gallery/scripts/pack-curated-modules.py` (fixed entry timestamps, stored
entries, no directory records) into `modules-showcase.zip`. Rejected:
zip-only (unreviewable code) and a runtime directory fetch (the web boot
mounts one zip, F6a).

**D3 — Explicit hooks, not globals.** The entry calls
`efx.registerUpdateHook`/`registerRenderHook`, exercising the documented
hook model; `main.js`-style global sugar stays covered by the golden scenes.

**D4 — No RNG, bounded work.** Orbit motion is a pure function of time and
JSON parameters; geometry is created once at load. The sample is a fixed
handful of meshes, so the headless smoke is deterministic and cheap.

**D5 — Data in JSON, behavior in JS.** `data/scene.json` carries the
background, camera, lights, core spin, and orbit layout (a JSON module);
`lib/orbit.js` maps `(t, orbit)` to a position; `lib/palette.js` builds Phong
materials and shades colors. This keeps the JSON module meaningful rather
than a token parse.

## Risks / Trade-offs

- [Sources and pack drift] → the packer is deterministic and documented in
  `CREDITS.md`; a follow-up edit to a module must re-run the packer. Accepted
  cost, matching the existing curated packs.
- [A curated sample that needs an asset pack is copied into the site by
  `gen-catalog.mjs`, and `manifest.json` must name the pack] → the added
  manifest entry includes `"assets": "modules-showcase.zip"`; the catalog
  build fails loudly if the pack is missing.
- [The gallery smoke runs the first eight catalog ids] →
  `curated:modules-showcase` sorts by id; add the sample and let the smoke
  exercise whatever falls in its window (the desktop smoke case is the
  deterministic module-graph check).
