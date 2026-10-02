# 0052 — Surface-less run modes create CPU-only resources (no deferred upload queue)

Status: Accepted (2026-10-02, change `collapse-pre-gpu-queue`)

## Context

`entry-after-gpu-init` (ADR 0016 amendment) moved the entry script's evaluation
after `efx_render_install_sink` in every run mode that has a rendering surface.
That left the engine's deferred pre-GPU upload queue reachable only from the
surface-less modes — desktop `--script` and the web Node harness — which never
render. The queue (`flush_pending_uploads`, the texture `pending` buffer, and
the mesh/render-target no-sink deferrals) therefore copied pixel data that was
never uploaded or read. It also carried an exception: `efx_render_white_texture`
refused to create without a sink, so `efx.graphics.whiteTexture` threw in
`--script` even though the `2d-layer` spec promises it is "usable in any draw"
and "valid for the whole run". ADR 0034 referred to the queue as the
"deferred pre-GPU-surface queue path".

## Decision

In run modes with no rendering surface, the render module creates **CPU-only
resources** through the same creation path it uses when a sink is present.
`src/render/render_texture.c` `efx_render_texture_create` uploads when a sink
exists and otherwise creates a live slot with `native = NULL` and **no pixel
copy**; render targets hold no native object and meshes retain only their
interleaved CPU geometry (needed by skinning and F12 colliders). Surface-less
resources report their size and participate in recorded draws, but nothing is
uploaded or rendered. `efx_render_white_texture` uses this same path and marks
its slot permanent, so `efx.graphics.whiteTexture` is available in every run
mode.

The deferred upload queue is removed: `flush_pending_uploads` and the texture
`pending` field are gone, `efx_render_install_sink` is a plain assignment, and
`efx_render_shutdown` no longer frees pending buffers. Surface-less resources
remain the same GC-finalized opaque classes with explicit `destroy()`
(ADR 0011/0012) as in GPU modes; only the absence of a native object differs.
This supersedes ADR 0034's "deferred pre-GPU-surface queue path" clause.

## Consequences

- `efx.graphics.whiteTexture` behaves identically in every run mode; the
  `--script` and web Node harnesses no longer need to avoid it, and samples no
  longer need lazy getters to dodge a throw.
- The engine has one resource-creation path. A future run mode that creates
  resources before installing a sink would silently get CPU-only resources, so
  the invariant is that surface-bearing modes install the sink before
  evaluation (ADR 0016).
- Surface-less textures no longer retain pixel bytes; size queries use slot
  dimensions and draws use the handle, so no observable behavior changes.
- Mipmaps and sampler options remain immutable creation state; a CPU-only
  texture reports them but does not upload.

## Rejected alternatives

- **Install a production mock/headless sink** so creation always takes the sink
  path (as `tests/catalog_runner.c` used to): adds a headless sink to the
  shipping engine and still allocates dummy native bytes for every created
  texture, for no observable benefit.
- **Keep the deferred queue** for the surface-less harnesses: it is exactly the
  machinery this change exists to remove, and it left `whiteTexture`
  inconsistent with its spec.
- **Give `--script`/web Node a real GPU context**: not portable (offscreen
  EGL/headless-gl) and contrary to ADR 0007's "no sokol initialization at all".
