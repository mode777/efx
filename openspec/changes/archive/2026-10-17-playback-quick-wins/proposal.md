# Proposal

## Why

An architecture review of the playback/shader layer (2026-10-17) found three
high-gain, low-effort improvements clustered in the same two files. Billboard
playback issues one draw call — with full pipeline + binding re-apply — per
record, while the equivalent 2D quad path already coalesces consecutive
records into batched runs (`efx_render_runs`), so billboard-heavy scenes pay
an order of magnitude more draws than they need. In the same playback path,
the `depthTest: false` option recorded by `drawBillboard` is never consumed:
every billboard draws through the depth-tested pipeline, so the specced
"draws over geometry" behavior (billboards spec, "depthTest false draws over
geometry" scenario) is not implemented and no test exercises it. Finally,
the mesh fragment shader samples all five material maps before the `unlit`
early-out, so the cheapest surface class — unlit, mapless, e.g. geometry
skyboxes — wastes four texture fetches per fragment.

## What Changes

- **Billboard run batching**: consecutive billboard records sharing resolved
  texture, blend mode, and depth-test setting are coalesced into one draw
  call from the shared dynamic vertex buffer, exactly mirroring the existing
  quad run plan (`efx_render_runs`). Playback order and rendered output are
  unchanged; runs break at any intervening non-billboard record.
- **Honor `depthTest: false` at playback**: add the missing depth-always
  billboard pipeline variants (`bill_pip` grows to `[blend][depth_test]`) and
  select per record from the recorded `depth_test` flag. The batching run key
  includes the flag, so batching and the fix land together consistently.
- **Unlit early-out fetch reorder** (`shaders/mesh.glsl`): move the
  ambient/specular/emissive map fetches below the `unlit` branch so unlit
  surfaces sample only the alpha mask and diffuse map. Rendered output is
  pixel-identical; the committed `shaders/mesh.h` is regenerated with the
  pinned sokol-shdc (ADR 0021).

No script-facing API, error-behavior, or name changes of any kind.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `billboards`: add a playback-batching requirement — consecutive billboard
  records sharing resolved texture, blend, and depth-test setting SHALL
  coalesce into one draw call with rendered output identical to per-record
  playback, and runs SHALL break at any intervening non-billboard record.
  The existing "Billboard depth and blend behavior" requirement text is
  unchanged: `depthTest: false` is already specced; this change makes the
  implementation comply and adds the first test coverage for it.

The `lighting` capability is touched implementation-only (fetch order inside
the one canned mesh shader); its requirements and scenarios are unchanged, so
it gets no delta. `docs/api/`, `gallery/src/api/efx.d.ts`, and
`docs/js-api.md` are untouched — no script-visible surface moves.

## Impact

- `src/render/render_records.c` (+`render_internal.h`/`render.h` if the run
  plan is generalized): build a billboard run plan alongside the quad runs.
- `src/platform/pipeline.c`: consume the billboard run plan in
  `play_records`/`draw_textured`; grow the billboard pipeline bank to
  `[blend][depth_test]` and select on the recorded flag.
- `shaders/mesh.glsl` + regenerated committed `shaders/mesh.h` (pinned
  sokol-shdc, `EFX_SHDC` CMake cache path; ADR 0021).
- Tests: unit coverage for the run plan and depth-test pipeline selection;
  first-ever test for `depthTest: false`; goldens re-verified pixel-identical
  except the new depthTest scene, which needs a one-time server-side capture
  (docs/verification-server.md recipe).
- Performance: billboard-heavy frames drop from N draws (+N pipeline/binding
  applies) to one draw per consecutive same-state group; unlit fragments skip
  3 texture fetches; skybox-class geometry benefits most.

**Docs**: No ADR — no new durable cross-cutting decision: batching is
sanctioned by ADR 0019's re-orderable display list and the 2d-layer
display-list requirement; the pipeline-variant mechanism is ADR 0039's; the
depthTest fix is compliance with the existing billboards spec. AGENTS.md and
docs/js-api.md are unchanged.

## Non-goals

- No build-time shader permutations or sokol-shdc `@option` variants
  (rejected twice, ADR 0026/0027 — stays rejected).
- No sort-key-based global state sorting of the display list (ADR 0019
  contract stays "playback order = record order"; `sort_key` stays dormant).
- No billboard instancing, no cross-mesh batching, no vertex-format changes.
- No particle-path changes (per-system draws are already batched; screen-space
  particle pipeline selection is untouched).
- No `lighting` spec or CPU-reference changes — the Phong math is identical.
