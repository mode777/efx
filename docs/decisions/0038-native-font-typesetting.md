# 0038 — Native font typesetting: fixed atlas baked in C, drawn as quads

Status: Accepted (2026-09, change `f8a-font-typesetting`; amended 2026-09: the
F8b `drawModel`/demo-pack slice is retired as obsolete — superseded by
multi-surface meshes, ADR 0024)

## Context

F8's text slice had a provisional shape: `loadFont` returning a JS-managed
font (an atlas Texture + a quad layout) with `drawText` as a pure-JS high-level
convenience, and ADR 0013 explicitly rejected a native Font. Two facts made
that shape unworkable as written: scripts have **no binary resource read**
(`loadText` is NUL-terminated text, `loadImage` decodes), so a pure-JS
rasterizer cannot obtain a `.ttf`'s bytes; and font metrics/kerning live
inside the parsed file, so JS layout would force a per-glyph metrics API
across the JS/C boundary. Meanwhile the renderer already had every primitive
text needs — a tinted, source-rect quad batched in the display list.

## Decision

Text is a **mid-level C facility**, not a pure-JS convenience, and the font is
two native-backed resources:

- `efx.loadFontData(path)` → **FontData**: a parsed TrueType/OpenType font
  (CPU, no GPU resource).
- `efx.createFont(fontData, opts)` → **Font**: a **fixed** glyph atlas baked
  once at `opts.size` from a codepoint set (default printable Latin-1), with
  optional baked `outline`/`shadow` variants, packed by the vendored
  `stb_rect_pack` and uploaded as one RGBA8 Texture (white RGB + coverage
  alpha, so glyphs tint by draw color). The atlas never grows.
- `efx.drawText` / `efx.measureText` are C-implemented, 2D-only, and opaque:
  they lay out the string and record glyph quads (shadow → outline → fill)
  through `efx_render_quad`; no glyph metrics or atlas cross the binding.
  Formatting is newlines, greedy word wrap, `left`/`center`/`right`/`justify`,
  and `top`/`middle`/`bottom` — no rich text, 3D text, or complex-script
  shaping.

`src/render/text.[ch]` is pure C (no sokol/quickjs; ADR 0003) and uses the
vendored `stb_truetype` (parse/metrics/kerning/rasterize) plus
`stb_rect_pack`; `sokol_fontstash.h` is rejected because its sokol port renders
through `sokol_gl.h`, a second renderer that bypasses the display list. This
supersedes ADR 0013's "font is a pure-JS construct / native Font rejected"
clause; the rest of 0013's taxonomy model stands.

## Consequences

- The renderer's existing quad path carries text with **no new shader and no
  new record type**; effects are extra quads, not a text shader.
- Layout and atlas baking are deterministic (sorted/deduped codepoints,
  deterministic pack order, integer/float arithmetic), so text golden images
  are stable across the four targets under ADR 0020.
- The atlas is fixed: no glyph-on-demand growth and no multi-size atlas in one
  `Font`. A new size or charset means a new `Font`; a caller needing more
  glyphs than the atlas holds gets a creation-time `Error`.
- `stb_truetype` does no hinting and no shaping; small text is soft and
  ligatures/Arabic/Indic are out of scope. FreeType/HarfBuzz remain the
  heavier alternatives if that ever changes.
- Future text work (MSDF/SDF, rich text) builds on the
  `loadFontData → createFont → drawText/measureText` contract without
  reshaping it. (The F8b `drawModel`/demo-pack slice is retired as obsolete;
  see the Status amendment.)

## Rejected alternatives

- **Pure-JS font over `loadImage`/`loadText`**: lost — no binary font read,
  no JS rasterizer, and duplicated layout across quickjs and the browser
  engine.
- **JS layout over a C metrics API**: lost — serializing the whole glyph table
  (or a per-glyph call) across the boundary buys nothing; C owns the metrics
  already.
- **`sokol_fontstash.h` + `fontstash.h`**: lost — requires `sokol_gl.h`, a
  parallel immediate-mode renderer that bypasses the display list; fontstash
  is kept only as an algorithmic reference.
- **Dynamic/on-demand atlas**: lost — non-deterministic pack order and larger
  code; a fixed baked atlas is enough for UI/game text and keeps goldens
  stable.
- **Channel-packed effects with a custom text shader**: lost — a new internal
  shader and a color-carrying record for fewer quads; ordered layers reuse the
  existing quad path.
- **FreeType / HarfBuzz**: lost — far too large for the pinned single-file
  vendoring policy; shaping is out of scope for this engine.
