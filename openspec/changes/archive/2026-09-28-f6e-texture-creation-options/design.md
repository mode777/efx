# Design

## Context

See `proposal.md` — Why. Relevant current state:

- `createTexture(imageData, opts?)` already accepts `{ wrap, filter }`
  (F6b). `src/api/api.c` and `src/web/entry.js` validate the option object
  by an explicit known-field allowlist; the web bridge mirrors the desktop
  binding by hand (`src/web/bridge.c`).
- The render sink contract is `create_texture(ud, w, h, rgba, wrap,
  filter)` (`src/render/render.h`). `src/render/render.c` validates and
  stores `wrap`/`filter` on a `tex_slot`; when no GPU surface exists yet
  (top-level `main.js` calls) it queues the raw pixels until frame start,
  so any new creation state must survive that deferred path too.
- The platform creates one `sg_image` with `.data.mip_levels[0]` and one
  cached `sg_sampler` per `(wrap, filter)` pair
  (`src/platform/pipeline.c`). Sokol treats mipmaps as a first-class
  *sampling* feature (`sg_image_desc.num_mipmaps`; in this pinned version
  separate `sg_sampler_desc.min_filter`, `.mag_filter`, and `.mipmap_filter`,
  plus `min_lod`/`max_lod`), but it does **not** *generate* the chain: an
  immutable image "must be fully initialized by providing a valid `.data`
  member", and `sg_image_data.mip_levels[]` is uploaded verbatim (`num_mipmaps
  > 1` with only level 0 leaves the rest undefined). There is no
  `sg_image_generate_mipmaps` and no `glGenerateMipmap` call in the vendored
  header.
- `efx.loadTexture` lives only in the shared JS prelude
  (`src/prelude/prelude.js`), embedded into both runtimes via
  `tools/gen_prelude.py` → `src/prelude/prelude.h`. The desktop runtime
  registers C functions in `src/runtime/runtime.c`; the web runtime builds
  the same surface in `src/web/entry.js`.

## Goals / Non-Goals

**Goals:**

- One texture-creation flow with options, symmetric with the mesh flow.
- Mipmaps as a deterministic, cross-target-identical texture creation
  state, default off, so existing goldens stay byte-identical.
- Keep the change contained to the existing texture path; no new resource
  type, no new script-visible query surface.

**Non-Goals:**

- GPU-side mip generation, per-level script control, or runtime
  regeneration.
- Mipmaps for render targets.
- glTF importer mipmap awareness (imported textures keep F6b behavior).
- Any compatibility shim for `loadTexture`.

## Decisions

### D1 — Mip chain is generated on the CPU at creation

`createTexture(..., { mipmaps: true })` builds the full chain down to 1×1
on the CPU with a 2×2 box filter (each level's texel is the rounded average
of the corresponding 2×2 block below it), then uploads all levels in one
`sg_image` (`num_mipmaps = level_count`, every level filled). Non-power-of-
two dimensions are supported by flooring each level's size (`max(1, >>1)`)
until 1×1.

- **Why:** sokol-gfx uploads exactly the levels it is given and never
  generates them itself, so generation must live in the engine; a fixed CPU
  filter is bit-identical on GL, D3D11, Metal, and WebGL, which is what the
  golden-image gate (ADR 0020) depends on. (Sokol's native mipmap support
  covers *sampling* — see D2 — not generation.)
- **Alternatives rejected:** `glGenerateMipmap` (GL-only, and sokol-gfx
  exposes no hook to call it); a render-pass downsample chain (needs extra
  engine-owned targets and a resolve per level — far more moving parts for
  no quality this milestone needs); leaving mipmaps to the importer
  (scripts still cannot control minification of authored textures).
- Box filter over higher-order filters: predictable, cheap, and adequate;
  a better filter is a later, spec-visible change if it ever matters.

### D2 — `mipmaps` is a boolean; `filter` drives sokol's filters

The option is `mipmaps?: boolean` (default `false`), not a set of
mipmap-aware filter strings. Sokol already exposes a dedicated
`sg_sampler_desc.mipmap_filter` (separate from `min_filter`/`mag_filter`),
so enabling mipmaps means setting the sampler's `mipmap_filter` from the
same `filter` value: `'linear'` → `SG_FILTER_LINEAR` (trilinear when
combined with a linear `min_filter`), `'nearest'` → `SG_FILTER_NEAREST`;
`min_filter` and `mag_filter` stay the chosen `linear`/`nearest`, so
magnification never uses a mip level. With `mipmaps: false` the engine keeps
its current `min_filter = mag_filter = filter` sampler unchanged. (The
engine API stays boolean-plus-`filter` — it does not surface
`mipmap_filter` directly.)

- **Why:** sokol already separates mip filtering from min/mag filtering, so
  this is a direct mapping; keeping the consumer API boolean-plus-filter
  avoids leaking that split (or `min_lod`/`max_lod`) into a fixed-function
  API.
- **Alternatives rejected:** exposing `mipmap_filter` (or all six
  min/mag/mipmap combinations) directly (backend vocabulary, doubles the
  validation and type-test surface); a mode string
  (`'none'|'linear'|'nearest'`) — the boolean composes with the existing
  `filter` more cleanly and matches the user's mental model.

### D3 — Creation state flows through the sink and the sampler cache

`efx_render_texture_create` and the `create_texture` sink callback gain a
`mipmaps` argument; `tex_slot` and `pipe_tex` store it; the deferred
(no-surface) queue path carries it into the eventual upload. The platform's
sampler cache is keyed by `(wrap, filter, mipmaps)` instead of
`(wrap, filter)`, and `pipe_create_texture` fills `num_mipmaps` plus every
`mip_levels[i]` when requested. `efx_render_texture_sampler` gains an
optional `out_mipmaps` (existing callers pass `NULL`), so glTF tests and
future diagnostics can read the full creation state.

- **Why:** the sink is the single boundary between the render layer and
  sokol; threading one more immutable creation field through it keeps the
  design uniform with `wrap`/`filter` and preserves the deferred-upload
  correctness.
- **Alternatives rejected:** packing `mipmaps` into the `filter` int
  (opaque, error-prone across the bridge boundary); generating mipmaps in
  the render layer before the GPU exists (there is no GL context there).

### D4 — Remove `loadTexture` from the prelude; no shim

Delete the `efx.loadTexture` assignment in `src/prelude/prelude.js` and
regenerate `src/prelude/prelude.h` (the Linux gate fails on drift via
`gen_prelude.py --check`). No deprecation wrapper is added: an unknown
`efx.loadTexture` is a `TypeError` like any other missing property, which
is the clearest signal.

- **Why:** the whole point is one flow; a shim would keep two.
- **Alternatives rejected:** keeping a shim that forwards
  `createTexture(loadImage(path))` (defeats the consistency goal, and the
  gallery type document would advertise a function the project no longer
  wants); leaving `loadTexture` but adding options (two near-identical
  signatures).

### D5 — `mipmaps` validation is strict and eager

The option object keeps its allowlist (`wrap`, `filter`, `mipmaps`); a
non-boolean `mipmaps` throws `TypeError`, consistent with the existing
wrong-type handling. Both bindings (`api.c` and `entry.js`) validate
identically before any upload, so a rejected call records/creates nothing.

## Risks / Trade-offs

- **Memory/time cost of chains** → chains only build when explicitly
  requested (`mipmaps: true`); the extra cost is bounded by 4/3 of the base
  image and the filter is a straight CPU pass.
- **CPU generation is a new hot path for large textures** → it runs once
  at creation, is O(4/3 · pixels), and is not in the frame loop; the option
  defaults off so unaffected scenes pay nothing.
- **Golden re-baselining** → only the new mipmap scene is added; every
  pre-existing scene omits `mipmaps`, so their output must stay
  byte-identical, which the gate enforces.
- **Removing `loadTexture` breaks in-tree callers** → `load_png` golden,
  `texture-showcase` sample, and `s_6a_resource.js` are updated in the same
  change; the golden's rendered output is unchanged (same texture, same
  sampler).
- **Web/desktop parity drift** → the option parsing is mirrored by hand;
  the existing desktop-vs-web compare harness (`run_web_compare.mjs`) and a
  shared smoke case cover it.

## Migration Plan

Not applicable — the engine has no released consumers. In-tree callers are
migrated in the same change; the F6a golden and smoke scripts are the only
call sites and are rewritten to
`efx.createTexture(efx.loadImage(path), opts)`.
