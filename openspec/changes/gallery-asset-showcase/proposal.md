# Proposal

**Roadmap position:** Cross-cutting showcase/tooling infrastructure — no
feature milestone. It adds curated gallery content that presents the
already-shipped F6a/F6b asset-loading API with real assets; it does not
start or reopen a ladder milestone. Precedent: the archived
`web-gallery` change ("Cross-cutting showcase/tooling infrastructure — no
feature milestone").

## Why

The gallery's curated showcase set is how visitors meet the API, but the
two asset-loading features that shipped in F6 are only demonstrated by
tiny synthetic golden scenes: `load_png` loads an 82-byte checker and
`gltf_import` loads a 1.9 KB quad. They prove the mechanics and pin
regressions, but they advertise nothing. A visitor cannot see a real
texture or a real authored mesh render under the engine's Phong lighting.

The public sample models and textures that would make these features
legible are readily available under permissive licenses, and the gallery
already has the host channel (`__efx_assets`) to mount an asset pack. The
missing piece is that only golden scenes can ship an asset pack today;
the curated path ignores `Sample.assets`.

## What Changes

- **Curated asset packs.** A curated manifest entry may name an asset
  pack; `gen-catalog.mjs` copies it into `gallery/public/samples/` and
  sets the sample's `assets` URL, exactly as it already does for golden
  scenes. The runner's existing `__efx_assets` mount then makes
  `load*` work for the sample on web and desktop (`player <zip>`).
- **A texture showcase sample.** A curated sample that loads a real
  CC0 image with `loadTexture`, shows it full and via `sourceRect`, and
  tiles it through a repeat sampler on a 3D surface — the F6a path end
  to end.
- **A glTF static-mesh showcase sample.** A curated sample that loads a
  real CC0 glTF model with `loadMeshData`, lights it, and spins it — the
  F6b path end to end (base-color map, PBR→Phong material, no normal or
  occlusion maps).
- **Committed CC0 asset packs plus provenance.** Each showcase ships a
  small zip built from the source asset files, with a committed
  `CREDITS.md` recording author, source URL, license, and modifications
  for every file. Assets are optimized (external `.gltf` + downscaled
  base-color/metal-rough textures; the normal and occlusion maps the
  importer ignores are dropped) to keep the bundle and the repository
  small.
- **Generated catalog refresh.** `generated.json` is regenerated so the
  new samples appear, and the gallery smoke test exercises them.

## Capabilities

### New Capabilities
<!-- None: this change adds content, not new spec-level behavior beyond the
     gallery's existing capability. -->

### Modified Capabilities

- `web-gallery`: the sample catalog must support asset packs on curated
  samples (not only golden scenes) and must build/deploy them so those
  samples load their resources on the web and under the player.

## Impact

- **Code/content:** `gallery/scripts/gen-catalog.mjs` (curated asset
  handling), `gallery/samples/curated/` (manifest, two new sample
  scripts, source assets, `CREDITS.md`), `gallery/public/samples/`
  (generated zips), `gallery/src/samples/generated.json` (regenerated).
- **Docs:** `AGENTS.md` gallery bullet updated to note curated asset
  packs. No `docs/js-api.md` change and no `gallery/src/api/efx.d.ts`
  change — the script-facing API is unchanged. No ADR: the asset-pack
  convention is a content/tooling detail under the existing gallery and
  resource-root contracts (ADR 0031), not a cross-cutting invariant.
- **Verification:** `tools/run_gallery_smoke.mjs` (web, runs on the
  verification server and in the gate's browser job) walks the catalog
  and fails on any sample console error, so it exercises the new
  samples' asset loads for free. No golden baselines change.

## Non-goals

- No engine, renderer, or script-API change; no new `efx` function.
- No new golden scenes — real assets do not belong in the deterministic
  golden suite.
- No scene graph, `loadModel`, skinning, or animation playback (F7/F8).
- No non-commercial or non-standard-license assets (e.g. the CC-BY-NC
  DamagedHelmet or the SCEA Duck); CC0 only.
- No gallery UI for credits — provenance lives in a committed
  `CREDITS.md`, which CC0 does not legally require.
