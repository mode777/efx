# Design

## Context

See `proposal.md` — Why. This stage follows `entry-after-gpu-init`, which made
every GPU run mode evaluate the entry after `efx_render_install_sink`, leaving
the deferred upload queue reachable only from the surface-less harnesses
(`--script`, web Node) — modes that never render.

The queue lives in `src/render/render_texture.c`: with no sink,
`efx_render_texture_create` copies the RGBA bytes into a `pending` buffer and
`flush_pending_uploads` uploads them when a sink is later installed. Meshes and
render targets have analogous no-sink branches. `efx_render_white_texture` is
the exception — it returns 0 when there is no sink instead of creating a slot,
so `efx.graphics.whiteTexture` throws in `--script`.

## Goals / Non-Goals

**Goals:**
- One resource-creation path: with a sink, upload; without, create a CPU-only
  resource.
- Make `efx.graphics.whiteTexture` behave identically in every run mode.
- Delete the deferred queue and its dead pixel copies.

**Non-Goals:**
- Creating a rendering surface in surface-less modes.
- Changing GPU-mode creation, option validation, or error text.
- Removing the retained mesh CPU geometry used by skinning and F12 colliders.

## Decisions

### D1 — "No sink" means CPU-only, not a headless mock sink
With no sink, a texture slot is created live with `native = NULL` and no pixel
copy; render targets and meshes hold no native object. No headless sink is
installed.
*Alternatives:* installing a production mock sink (as `tests/catalog_runner.c`
does) so creation always takes the sink path — rejected: it adds a headless
sink to the shipping engine and still allocates dummy native bytes for every
created texture, including large ones, for no observable benefit. Keeping the
queue — rejected: it is the machinery this change exists to remove.

### D2 — `whiteTexture` uses the normal creation path
`efx_render_white_texture` drops its `if (!R.sink) return 0;` guard and calls
`efx_render_texture_create` unconditionally, marking the slot permanent. The
"white texture unavailable" error remains only for a genuine creation failure
(OOM / real GPU upload failure) in a surface-bearing mode.

### D3 — Delete the texture queue; keep the mesh CPU geometry
Remove `flush_pending_uploads`, the texture `pending` field, and the
`pending`-aware release paths; `efx_render_install_sink` becomes a plain sink
assignment and `efx_render_shutdown` stops freeing pending buffers. The mesh
`pending` struct stays: it is the retained interleaved CPU copy that backs
skinned bind data and `createStaticMesh` colliders (F7/F12), not a queue — only
the no-sink upload-deferral in `efx_render_mesh_create` is removed.

### D4 — Unit tests that exercise the transition are rewritten
`render_tests.c` cases `texture_queued_handles` and `mesh_pending_upload`
assert the old "create without a sink, install, flush" transition; they become
assertions that no-sink creation succeeds as CPU-only (size/query/alive) with
no native object. The `white after shutdown` assertion that white creation
fails without a sink becomes an assertion that it succeeds as a CPU-only
resource.

### D5 — Resource exposure is unchanged
No new resource type is introduced. Surface-less resources are the same
GC-finalized opaque classes with explicit `destroy()` (ADR 0011/0012) as in GPU
modes; only the absence of a native object differs.

## Risks / Trade-offs

- [Removing the transition means any future mode that creates resources before
  installing a sink would silently get CPU-only resources] → Stage 1 guarantees
  the sink exists before evaluation in every GPU mode; document the invariant
  and keep a debug assertion that a sink-less create is intentional.
- [Surface-less textures no longer retain pixel bytes] → Verified that no code
  reads `pending` bytes except `flush_pending_uploads`; size queries use the
  slot dimensions and draws use the handle.
- [Handle-sequence and flush unit tests change] → Rewrite them to the new
  contract; they are internal-mechanism tests, not spec requirements.
- [Golden images could shift if GPU-mode creation changes] → It does not; a
  pixel diff is a regression, never a re-baseline.

## Migration Plan

Land after `entry-after-gpu-init`. Rollback is reverting this change: the queue
and its tests are self-contained in the render module. No data or API migration
is required.
