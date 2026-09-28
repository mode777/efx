# 0034 — Texture mipmaps: CPU-generated chain, boolean creation option

Status: Accepted (2026-09, change `f6e-texture-creation-options`)

Supports: ADR 0020 (golden-image verification — determinism across the
four backends); ADR 0028 (`createTexture` is the one CPU→GPU upload path);
ADR 0031 (the synchronous provider and resource API); ADR 0032 (per-texture
sampler creation state, which this extends and whose "mipmaps out of scope"
line it amends).

## Context

F6b gave `createTexture` per-texture `wrap`/`filter` samplers but explicitly
deferred mipmaps, so minified textures alias. Sokol treats mipmaps as a
first-class *sampling* feature — `sg_image_desc.num_mipmaps` and, in this
pinned version, a dedicated `sg_sampler_desc.mipmap_filter` separate from
`min_filter`/`mag_filter` — but it never *generates* the chain: an immutable
image "must be fully initialized by providing a valid `.data` member", and
`sg_image_data.mip_levels[]` is uploaded verbatim. There is no
`sg_image_generate_mipmaps` and no `glGenerateMipmap` hook. The engine also
ships one texture-creation flow (`createTexture`), so whatever it does must
be identical on GL, D3D11, Metal, and WebGL.

## Decision

- **Mipmaps are a boolean creation option.** `createTexture(imageData,
  { mipmaps })` defaults to `false`; `true` builds a full chain down to 1×1
  (NPOT levels floor each step: `max(1, n/2)`). An absent/false value and
  any existing texture creation stay byte-identical to F6b.
- **The chain is generated on the CPU with a 2×2 box filter** at creation,
  and all levels are uploaded in one `sg_image`. `src/platform/pipeline.c`
  owns generation; `src/render/render.c` carries the immutable `mipmaps`
  flag through the sink (including the deferred pre-GPU-surface queue path)
  and through the sampler cache, now keyed by `(wrap, filter, mipmaps)`.
- **`filter` keeps its meaning and drives the mipmap filter.** With
  `mipmaps: true`, the sampler's `mipmap_filter` is set from `filter`
  (`linear` → trilinear, `nearest` → nearest-mipmap); `mag_filter` stays
  plain, so magnification never samples a mip level. The consumer API does
  not surface `mipmap_filter`, `min_lod`, or `max_lod`.
- **No script-visible mip surface.** There is no per-level control, no LOD
  bias, and no query property; `mipmaps` is creation state consumed at
  upload.

## Consequences

- Mip generation is deterministic and backend-independent (a fixed CPU
  filter), so the golden gate (ADR 0020) can compare across targets; a
  future change to the filter is a spec-visible re-baseline, not an
  implementation detail.
- The cost is bounded by 4/3 of the base image and paid once at creation,
  only when requested; the option defaults off so no existing scene slows
  down or changes output.
- The sampler cache and the creation-state tuple must carry the flag; any
  future texture creation path (e.g. the glTF importer) either passes
  `false` or explicitly opts in.
- RenderTargets have no mipmaps, and the glTF importer keeps collapsing
  mipmap sampler filters to `linear`/`nearest` (imported textures get no
  chain) until a later change says otherwise.

## Rejected alternatives

- **`glGenerateMipmap` / backend auto-generation.** GL-only, backend-
  dependent, and not exposed through sokol-gfx; it would break the
  byte-identical golden comparison across the four targets.
- **A GPU render-pass downsample chain.** Needs extra engine-owned targets
  and a resolve per level — far more moving parts for quality this
  milestone does not need.
- **Exposing mipmap-aware filter values** (e.g. `'linear-mipmap-linear'`)
  or `min_lod`/`max_lod`. It leaks backend filter vocabulary into a
  fixed-function API and doubles the validation and type-test surface; the
  boolean composes with the existing `filter`.
- **A `mipmaps` mode string** (`'none'|'linear'|'nearest'`). Redundant with
  `filter` and less clear than a boolean.
