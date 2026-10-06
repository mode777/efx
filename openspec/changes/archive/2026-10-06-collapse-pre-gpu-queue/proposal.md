# Proposal

## Why

`entry-after-gpu-init` (this change's predecessor) evaluates the entry script
after the rendering surface exists in every GPU run mode, so the engine's
deferred pre-GPU upload queue is no longer reachable there. Its only remaining
consumers are the surface-less harnesses (`--script` and the web Node harness),
which never render — the queued pixel bytes are copied and freed without ever
being uploaded or read. The queue also carries an inconsistency: `whiteTexture`
refuses to create without a sink, so it throws in `--script` even though the
`2d-layer` spec says it is "usable in any draw" and "valid for the whole run".
Collapsing the queue removes dead machinery and makes engine-owned resources
behave identically in every run mode.

## What Changes

- **Surface-less creation is CPU-only.** With no sink, `efx_render_texture_create`
  creates a live slot with `native = NULL` and **no pixel copy**; render targets
  and meshes likewise hold no native object. The resource still reports its size
  and participates in recorded draws; it is simply never uploaded or rendered.
- **`efx_render_white_texture` loses its no-sink early return** and goes through
  the same creation path, so `efx.graphics.whiteTexture` is available in every
  run mode, including `--script` and the web Node harness.
- **Remove the deferred queue:** delete `flush_pending_uploads`, the texture
  `pending` field and its free/release paths, and the no-sink upload-deferral in
  the mesh and render-target creation paths. `efx_render_install_sink` becomes a
  plain sink assignment.
- **Keep the mesh CPU geometry.** `mesh_pending` is retained (it backs skinned
  bind data and F12 static-mesh colliders); only its upload-deferral is removed.
- **Tests and samples:** update the queued-handle/pending-flush unit cases, the
  `--script` validation comments, and the audio showcase's lazy `white()` getter
  (which exists solely to dodge the throw).
- **BREAKING (bug fix):** surface-less scripts that observed
  `whiteTexture` throwing now receive a valid 1×1 Texture. The `2d-layer` spec
  already required the latter, so this makes the implementation match the
  contract.

Depends on `entry-after-gpu-init`: removing the queue is only safe once no GPU
run mode creates resources before the sink.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `2d-layer`: `efx.graphics.whiteTexture` is available and usable in every run
  mode; in surface-less modes it (and other created resources) are CPU-only and
  never rendered.
- `player-runtime`: surface-less run modes (`--script`, web Node) create
  resources as CPU-only — creation and size queries succeed, draws are recorded,
  nothing renders, and engine-owned resources are available.

## Impact

- **Code:** `src/render/render_texture.c` (drop `flush_pending_uploads`/`pending`,
  simplify `whiteTexture`), `src/render/render_records.c` (install/shutdown),
  `src/render/render_mesh.c` and `src/render/render_target.c` (drop deferral),
  `src/render/render_internal.h`, `src/api/api_2d.c`/`src/web/js/audio.js`
  (no-sink error text removal).
- **Tests:** `tests/unit/render_tests.c` (`texture_queued_handles`,
  `mesh_pending_upload`, `white after shutdown`), `tests/unit/api_tests.c`,
  `tests/scripts/s_2d_validation.js`, `tests/scripts/s_5a_validation.js`,
  `tests/catalog_runner.c` (its mock sink becomes unnecessary).
- **Docs/ADRs:** new **ADR 0052** — surface-less resources are CPU-only and the
  engine has one resource-creation path; note that ADR 0034's
  "deferred pre-GPU-surface queue path" mention is superseded; update the
  `AGENTS.md` current-state map. No `gallery/src/api/efx.d.ts` or `docs/api/`
  change (no API shape change).
- **Milestone:** no roadmap milestone — post-roadmap architecture change
  (F1–F14 complete); follows ADR 0016's adopted reorder.

## Non-goals

- Reintroducing any GPU context for `--script` or the web Node harness.
- Changing the resource-creation API, its option bags, or its error text for
  GPU run modes.
- Changing how meshes retain CPU geometry for skinning or physics colliders.
- Re-baselining golden images; a pixel diff is a regression.
