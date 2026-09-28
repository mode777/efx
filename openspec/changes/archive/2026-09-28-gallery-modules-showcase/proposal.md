# Proposal

**Roadmap position:** Cross-cutting showcase/tooling infrastructure — no
feature milestone. It adds curated gallery content that presents the
already-shipped F10 CommonJS module runtime; it does not start or reopen a
ladder milestone. Precedent: the archived `gallery-input-showcase` and
`gallery-asset-showcase` changes.

## Why

F10 shipped a synchronous CommonJS module system (`require`, module caching,
cycles, `__esModule` interop, JSON modules) and documented it in
`docs/js-api.md`, but the only place it is exercised is headless tests. Every
other shipped feature has a visitor-facing gallery sample, while the change
that lets authored code be split across files has none. A visitor cannot see
`require` resolving from the resource root, a JSON module, or a required
module registering its own hooks without reading the spec.

## What Changes

- **A curated modules showcase sample.** `gallery/samples/curated/
  modules-showcase.js`: a small 3D scene whose code is split across files in
  the sample's asset pack and composed with synchronous `require` —
  - a relative module (`./lib/palette.js`) supplying per-surface Phong
    materials;
  - a deterministic extension-fallback require (`./lib/orbit`) resolving the
    same graph without an explicit `.js`;
  - a JSON module (`./data/scene.json`) supplying the background, camera,
    lights, and orbit layout;
  - the entry registering `update`/`render` hooks from the composed data.
- **An authored asset pack.** `gallery/samples/curated/modules-showcase.zip`
  holds the required modules at the archive root (no third-party assets);
  the committed readable sources live beside it in
  `gallery/samples/curated/modules/`.
- **Manifest + catalog.** A `manifest.json` entry (category `Showcase`) and a
  regenerated `gallery/src/samples/generated.json`. A desktop smoke test
  loads the entry against the pack so the module graph is exercised in the
  native gate.
- **Docs.** A short "Script modules" pointer in `README.md` and a mention of
  the sample in `AGENTS.md`. `docs/js-api.md` already documents the F10 model
  (delivered with the feature); no API or type-document change.

## Capabilities

### New Capabilities
<!-- None: content, not new spec-level behavior. `web-gallery` already
     requires the catalog to include curated showcase samples. -->

### Modified Capabilities
<!-- None: no behavior change. skip_specs is set. -->

## Impact

- **Content:** `gallery/samples/curated/modules-showcase.js` (new),
  `gallery/samples/curated/modules/` (new authored sources),
  `gallery/samples/curated/modules-showcase.zip` (new generated pack),
  `gallery/scripts/pack-curated-modules.py` (new deterministic packer),
  `gallery/samples/curated/manifest.json` (one entry),
  `gallery/samples/curated/CREDITS.md` (recipe note).
  `gallery/src/samples/generated.json` is gitignored and regenerated.
- **Tests:** one `add_player_test` smoke case in `tests/CMakeLists.txt`
  loading the sample against its pack.
- **Docs:** `README.md` (module format pointer); `AGENTS.md` gallery bullet.
- **Verification:** gallery `check`/build, the server gallery smoke, and the
  four-target gate (the new desktop smoke case runs in the native jobs).
