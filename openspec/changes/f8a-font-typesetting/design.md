# Design

## Context

See `proposal.md` for motivation. The design sits on these existing facts:

- Rendering is a record → sort → playback display list (`src/render/render.[ch]`,
  ADR 0019). `efx_render_quad` already takes a source rect, per-quad tint,
  rotation, scale, and origin, and consecutive quads batch by
  `texture + blend`. An RGBA8 atlas drawn as tinted quads needs **no new
  record type and no new shader**.
- The render module is pure C — no sokol, no quickjs (ADR 0003 module walls).
  GPU work goes through the sink vtable; textures are created with
  `efx_render_texture_create` and referenced by opaque handles.
- Script resources are GC-finalized opaque classes (ADR 0011/0012) and are
  cataloged in `docs/js-api.md` + `gallery/src/api/efx.d.ts`. ADR 0013
  explicitly rejected a native Font and declared the font a pure-JS construct;
  this change supersedes that clause.
- Scripts have no binary resource read (`loadText` is NUL-terminated text,
  `loadImage` decodes), so a pure-JS rasterizer is not viable.
- Both bindings must stay in lockstep: `src/api/api.c` (quickjs) and
  `src/web/bridge.c` + `src/web/entry.js` (Emscripten).
- Verification is golden-image based on the display targets plus headless
  unit tests everywhere (ADR 0020), and the milestone gate is four-target.

## Goals / Non-Goals

**Goals:**

- A small, deterministic, dependency-light native text path: parse → bake →
  layout → quads.
- A script API that mirrors the existing data→resource composition
  (`loadImage`→`createTexture`, `loadMeshData`→`createMesh`).
- Fixed, reproducible atlases so goldens are stable.
- Formatting that covers real UI/game text: newlines, wrapping, L/C/R/justify,
  top/middle/bottom, with optional baked outline/shadow.

**Non-Goals:**

- Rich text, complex-script shaping, hinting, SDF/MSDF, variable fonts, 3D text
  (see proposal non-goals).
- Exposing glyph metrics or the atlas to scripts.
- A general font-atlas toolkit; only what text drawing needs.

## Decisions

### D1 — Vendor `stb_truetype` + `stb_rect_pack`; use fontstash only as reference

`stb_truetype.h` provides glyph rasterization, font/vertical metrics, and
kerning (legacy `kern` + basic GPOS) with no external dependencies; it is the
same public-domain/MIT stb family already vendored for `stb_image`. Its
limitations (no hinting, no shaping) match the PS2-era target and are
acceptable. `stb_rect_pack.h` gives a deterministic shelf packer for the fixed
atlas.

Alternatives and why they lost:

- **`sokol_fontstash.h` + `fontstash.h`** — the sokol port renders through
  `sokol_gl.h` (a second, immediate-mode renderer), which bypasses the display
  list and would mean vendoring `sokol_gl` too. Architecture mismatch.
- **`fontstash` alone as the engine** — its atlas is dynamically grown and its
  draw is callback/immediate; both fight the fixed-atlas + display-list design.
  Retained as an algorithmic reference for packing/line-breaking.
- **`libschrift`** — smaller, but TrueType-only (no CFF/OTF) and no kerning.
  Kept as a fallback candidate.
- **FreeType** — best quality (hinting) but ~200k LOC and its own build; breaks
  the pinned single-file vendoring policy.
- **HarfBuzz** — complex shaping is out of scope for this engine.
- **MSDF (msdfgen/msdf-atlas-gen)** — resolution independence is attractive but
  needs an SDF evaluation shader and a C++ offline tool; deferred.

### D2 — Typesetting and drawing are C (variant B), not pure JS

The project's layering rule puts high-level conveniences in pure JS, but text
is a mid-level facility like `drawQuad`/`drawMesh`: it emits display-list
records and needs the parsed font. Doing layout in JS would require either a
binary read plus a JS rasterizer (impossible today, slow, duplicated across
quickjs and the browser engine) or serializing the whole glyph table across the
boundary. C owns parse/bake/layout/draw; the pure-JS high-level layer remains
for conveniences such as `drawModel` (a later F8 slice). This is recorded as an
explicit layering clarification in the `js-api` delta.

### D3 — Compose `loadFontData` → `createFont`

`loadFontData(path)` returns a `FontData` (parsed font, CPU only);
`createFont(fontData, opts)` bakes the atlas and returns a `Font`. This mirrors
`loadImage`→`createTexture` and `loadMeshData`→`createMesh`, lets one file back
several fonts (sizes/effects) without re-reading, and keeps the loader a pure
C resource loader. The provisional `loadFont` convenience is removed.

### D4 — Fixed atlas: one size, a charset, no growth

`createFont` bakes exactly the requested glyph set at the requested size once.
Defaults to printable Latin-1 (U+0020–U+007E, U+00A0–U+00FF); `glyphs` replaces
the set. A request that does not fit fails at creation. This is deterministic
(pack order depends only on the input set) and keeps goldens stable. Alternatives
considered: dynamic on-demand atlas (fontstash-style — non-deterministic pack
order, larger code), multi-size atlas (one `Font` per size instead), MSDF
(deferred). A codepoint with no baked glyph falls back to the baked `?` when
present, otherwise draws nothing with zero advance.

### D5 — One RGBA8 atlas, white + coverage alpha, effects as ordered layers

The atlas stores each glyph variant as white RGB with coverage in alpha, so the
existing quad shader tints it by the draw color. Outline and shadow are baked as
**separate variants** (a dilated ring and a blurred silhouette) in their own
atlas regions; `drawText` records them in layer order (shadow → outline → fill)
with their respective colors. This avoids a text-specific shader and any new
record type. Rejected: packing fill/outline/shadow into R/G/B and compositing in
a custom text shader (fewer quads, but adds an internal shader and a color-carrying
record); separate atlas textures per layer (more bindings, no benefit).

### D6 — Layout algorithm

Metrics come from `stb_truetype` at the baked scale (advance, kerning,
ascent/descent/lineGap). Hard breaks on `\n`; greedy word wrap when `width` is
set, breaking over-long words at a character boundary. `align` positions each
line within the block; `justify` widens inter-word space on every line except the
last (left-aligned) and requires `width`. `valign` positions the block relative
to the anchor. `scale` scales glyph quads and line advance; `rotation` rotates
each glyph about the anchor using `drawQuad`'s rotation/origin arguments. Bounds
are `{ width: widest line, height: lines × lineHeight × scale, lines }`.

### D7 — Module layout

New pure-C `src/render/text.[ch]` owns the `FontData`/`Font` structs, baking,
packing, layout, and quad emission. `stb_truetype`/`stb_rect_pack` implementations
compile once in dedicated TUs (`src/render/stb_truetype_impl.c`,
`src/render/stb_rect_pack_impl.c`) with warnings relaxed, like the existing
`stb_image_impl.c`. The atlas is uploaded through `efx_render_texture_create`
(the existing texture path, deferred-destroy semantics); text records go through
`efx_render_quad`. No sokol/quickjs crosses into `text.c`.

### D8 — Resource exposure and lifetime

`FontData` and `Font` are native-backed classes (ADR 0011/0012). `FontData` has
no query properties. `Font` exposes read-only `size`, `lineHeight`, `ascent`,
`descent`; its atlas Texture is engine-owned and **not** exposed, so scripts
cannot destroy it out from under the font. Both expose idempotent `destroy()`;
using a destroyed resource throws `TypeError`. The display list keeps the font
alive while recorded text is pending (the atlas texture's deferred release covers
playback).

### D9 — Error mapping

Wrong type / unknown field / missing required field → `TypeError`; out-of-range
numbers (non-positive `size`, negative `padding`, non-positive outline width or
shadow blur) → `RangeError`; missing/unparsable font, no resource root, or atlas
overflow → `Error`. This matches the existing `js-api` error convention.

### D10 — Binding parity

`src/api/api.c` registers two classes and four functions; `src/web/bridge.c`
exposes the corresponding C entry points and `src/web/entry.js` wraps them in
`EfxFontData`/`EfxFont` classes with the same names, semantics, and errors.
Both share the same C core, so no layout logic is duplicated.

### D11 — Docs and ADR

`docs/js-api.md` (F8 section + resource table + limits note),
`gallery/src/api/efx.d.ts` + type-test, `AGENTS.md` current state/roadmap,
`vision.md` (remove `drawText` from the pure-JS example list), and
`vendor/README.md` are updated in the same change. A new
`docs/decisions/0038-native-font-typesetting.md` records the durable decision
and supersedes ADR 0013's "font is a pure-JS construct / native Font rejected"
clause; it is indexed in `docs/decisions/README.md`.

## Risks / Trade-offs

- **Cross-platform rasterization/float determinism** → Mitigation: stb uses
  integer/float math with no libm-heavy paths; golden tolerance (ADR 0020)
  absorbs minor differences; the layout/bounds unit tests use the same CPU code
  on every target.
- **Atlas overflow at large sizes / large charsets** → Mitigation: fixed-size
  atlas with an explicit creation-time `Error`; the default Latin-1 set is small;
  size/charset are caller-controlled.
- **Web cost of one `drawQuad` crossing per glyph per layer** → Mitigation:
  text is UI-scale; layers are only recorded when effects are baked; a batched
  glyph primitive can be added later without an API change if it measures.
- **No hinting means soft small text** → Accepted for the PS2 aesthetic; the
  alternative (FreeType) is too heavy for the vendoring policy.
- **Font asset licensing for tests/gallery** → Mitigation: commit a CC0 font and
  record it in `gallery/samples/curated/CREDITS.md` like the other packs.
- **Golden re-baseline** → Mitigation: the new text goldens are captured
  server-side under llvmpipe first, per `docs/verification-server.md`.

## Migration Plan

No existing callers use `loadFont`/`drawText` (provisional only), so the only
"migration" is documentation: the provisional F8 entries are replaced by the
settled contract in `docs/js-api.md` and `gallery/src/api/efx.d.ts`. No data or
save-format migration. Rollback is deleting the change's code paths; nothing
existing changes behavior.

## Open Questions

- The specific CC0 font used by the goldens/showcase (implementation picks one
  and records provenance); no spec or API impact.
- Exact default outline/shadow visual parameters (baked geometry only; the API
  shape is fixed).
