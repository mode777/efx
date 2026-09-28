# Design

## Context

See `proposal.md` — Why. Current state that shapes the approach:

- The gallery catalog (`gallery/scripts/gen-catalog.mjs`) merges golden
  scenes from `tests/goldens/*/main.js` with curated entries from
  `gallery/samples/curated/manifest.json`. It already copies a golden
  scene's `assets.zip` into `gallery/public/samples/<name>.zip` and sets
  `Sample.assets`; the curated path does not.
- The runner (`gallery/public/runner.html`) already accepts `assets` over
  `postMessage` and sets `globalThis.__efx_assets` before booting the
  player; the web boot fetches and mounts that archive as the resource
  root (F6a, ADR 0031). Desktop mounts a directory or zip via
  `player <root>` / `--script <file> --root <dir|zip>`.
- The glTF importer (`src/resource/gltf.c`, ADR 0032) binds only the
  **base-color** texture to the diffuse map and the **emissive** texture
  to the emissive map. Normal, occlusion, and metallic-roughness
  textures are never decoded; metallic and roughness reach Phong as
  scalar factors (specular color / shininess). Node and scene transforms
  are not applied.
- `tools/run_gallery_smoke.mjs` walks the catalog in pinned headless
  Chrome and fails on any sample console error; it is run by
  `tools/verify_remote.py gallery` and the gate's browser job. Curated
  ids sort before golden ids, so new curated samples are among the first
  the smoke runs.

## Goals / Non-Goals

**Goals:**

- Let a curated gallery sample ship an asset pack and load it through the
  existing host/player resource-root contract.
- Add two teaching showcase samples — texture loading and glTF static-mesh
  loading — using real, permissively licensed assets.
- Keep the committed bytes and the built bundle small.
- Give the new packs provenance and player-level portability coverage.

**Non-Goals:**

- No engine, renderer, binding, or script-API change.
- No new golden scene or golden re-baseline.
- No scene graph, skinning, or animation.
- No gallery credits UI (provenance is a committed file).

## Decisions

### D1 — Curated samples are the home for the showcase

Add the showcase as curated entries, not golden scenes and not a separate
demo pack. Goldens stay deterministic and synthetic; the gallery is the
public showcase; a curated sample is already portable to the player.

*Alternatives considered:* real assets as golden scenes — rejected
(re-baseline on llvmpipe, cross-target decode/precision risk, repo/bundle
bloat for content that is not a regression). A separate `examples/` demo
pack — rejected (duplicates the gallery, which already has the host
channel and catalog).

### D2 — Asset-pack plumbing in `gen-catalog.mjs` only

A curated manifest entry gains an optional `assets` field naming a zip
relative to `gallery/samples/curated/`. `curatedSamples()` copies it to
`gallery/public/samples/<id>.zip` and sets `sample.assets`, mirroring the
existing golden handling. `Sample`, `Frame.svelte`, and `runner.html` are
unchanged — the host channel already works.

*Alternatives considered:* a hard-coded map inside `gen-catalog.mjs` —
rejected (opaque, not data-driven). Changing the runner to mount a
directory — rejected (the web host contract is a single archive).

### D3 — Commit the runtime zip; document provenance and recipe

Each showcase commits its runtime archive at
`gallery/samples/curated/<id>.zip` and a
`gallery/samples/curated/CREDITS.md` recording, per file, the author,
source URL, license, and the optimization applied. The zip is the source
of truth for the build; the recipe in CREDITS regenerates it from
upstream.

*Alternatives considered:* build the zip in `gen-catalog.mjs` — rejected
(Node has no built-in zip container; adding an archive dependency for two
static files is not worth it, and goldens already commit zips). Commit
unpacked sources only — rejected (the web host needs an archive).

### D4 — CC0-only assets

Every asset is CC0 1.0. CC0 carries no attribution obligation, so the
provenance file is good practice rather than a legal requirement, and no
gallery UI is needed.

*Alternatives considered:* CC-BY with an attribution surface — rejected
(no credits convention exists; it would add a `web-gallery` UI
requirement for no functional gain). CC-BY-NC (e.g. DamagedHelmet) —
rejected (non-commercial restriction). SCEA (Duck) — rejected
(non-standard license).

### D5 — Ship optimized `.gltf` + external textures

Ship the `.gltf` container with its geometry `.bin` and only the textures
the importer binds, downscaled to keep them small; drop normal,
occlusion, and metallic-roughness images (never decoded — see Context).
Chosen model: **Avocado** by Microsoft (CC0, Khronos glTF-Sample-Assets),
one mesh / one primitive. Optimized pack ≈ geometry 23 KiB + a
downscaled base-color PNG.

*Alternatives considered:* ship the upstream `.glb` unchanged — rejected
(7.9–13 MiB because it embeds the maps the engine discards). Generate a
bespoke mesh — rejected (less compelling and no longer a real glTF
sample). BoomBox / WaterBottle (also CC0) — kept as fallbacks if
Avocado's converted material renders poorly.

### D6 — Two separate samples

`texture-showcase` (category **Textures**) loads a CC0 image with
`loadTexture`, draws it full and via `sourceRect`, and tiles it through a
repeat sampler on a 3D surface. `gltf-showcase` (category **Assets**)
loads the optimized model with `loadMeshData`, lights it, and spins it.

*Alternatives considered:* one combined "asset loading" sample —
rejected (two unrelated API paths in one script teaches neither well).

### D7 — Show the imported material as-is

`gltf-showcase` draws the material the importer bound (base color →
diffuse, factors → specular/shininess) and tunes the lights, so the
pinned PBR→Phong conversion is what the visitor sees. If the visual check
during apply shows the default metallic=1/roughness=1 result is unflattering,
the sample may override a surface with `setMeshSurfaceMaterial` — which
also demonstrates the override API.

### D8 — Two verification layers

- **Web:** the existing gallery smoke runs the new curated samples (their
  ids sort first) and fails on any load error, in the gate's browser job
  and under `tools/verify_remote.py gallery`.
- **Native:** add committed player smoke tests, mirroring
  `smoke_6a_root_zip`, that run each gallery sample with
  `--script <sample>.js --root <sample>.zip` and expect exit 0. A failed
  `load*` throws and exits non-zero, so this proves the portability
  scenario on all four targets.

*Alternatives considered:* rely on the web smoke alone — rejected (the
spec asserts player portability; a committed native test is cheap and
matches the existing `--script --root` pattern). A dedicated test script
duplicating the loads — rejected (running the real sample is the faithful
check).

### Unmanaged resources

No new unmanaged resource type. The samples use the existing GC-finalized
`Texture` and `Mesh` classes, and imported textures are retained by the
`MeshData`/`Mesh` until destruction or `createMesh` consumption (F6b), so
the scripts need not hold texture handles.

## Risks / Trade-offs

- **PBR→Phong look** (metallic defaults to 1.0, roughness to 1.0) may
  render flat or over-shiny → tune lighting at apply and fall back to a
  material override (D7) or BoomBox/WaterBottle (D5). Validated visually
  before verification, not golden-tested.
- **Node transform not applied**: Avocado's mesh node carries a 180° Y
  rotation → irrelevant to a spinning display; noted so a future static
  pose is not mistaken for a bug.
- **Asset provenance/reproducibility**: a committed binary zip is opaque
  → `CREDITS.md` records exact upstream URLs, licenses, and the
  optimization recipe.
- **Git/bundle size**: optimized packs stay under ~1 MiB each; the
  texture pack is tens of KiB.
- **Smoke ordering**: curated samples run early in the gallery smoke; a
  broken asset load would fail the browser job → intended, and fixed
  before verification.
- **Test coupling**: native smoke tests reference
  `gallery/samples/curated/*` paths → acceptable; the reverse coupling
  (gallery reading `tests/goldens/`) already exists.

## Migration Plan

Additive content and one build-script extension; no migration. Rollback is
reverting the change and regenerating `generated.json`. The Pages
deployment picks the new samples up on the next `main` push; nothing else
is affected.

## Open Questions

- Whether to surface per-sample credits in the gallery UI. Deferred — CC0
  requires no attribution, and this does not change the specs, approach,
  or tasks; revisit only if non-CC0 assets are ever added.
