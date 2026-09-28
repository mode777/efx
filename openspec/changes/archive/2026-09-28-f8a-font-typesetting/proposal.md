# Proposal

**Roadmap position:** F8 (high-level JS + text) — this change is the text
slice of F8. F8 is the next ladder milestone after F7, whose four-target gate
is green; the orthogonal F9/F10 gates are also green. `drawText` and
`loadFont` are already named as F8 deliverables in `vision.md`,
`docs/js-api.md`, and the roadmap; this change settles their contract.

## Why

Text is the last major vision capability with no implementation, and the
provisional F8 sketch (`efx.loadFont(path)` + a pure-JS atlas + quad layout)
cannot actually work as written:

- There is **no binary resource read** exposed to scripts — `loadText`
  returns a NUL-terminated string and `loadImage` decodes internally — so a
  pure-JS TrueType rasterizer has no way to obtain the font file's bytes.
- Font metrics live inside the parsed font file; doing layout in JS would
  force a per-glyph metrics API across the JS/C boundary for no benefit.

The engine already has every primitive text needs (a texture atlas sampled
through `drawQuad`, source-rect quads, per-quad tint, display-list batching),
so the missing piece is a small, deterministic native typesetting path built
on a vendored rasterizer. Settling this now unblocks the rest of F8
(`drawModel`, demo pack) and gives the gallery its first text sample.

## What Changes

- **Vendor a rasterizer.** Add `stb_truetype.h` (glyph rasterization + font
  metrics/kerning) and `stb_rect_pack.h` (deterministic atlas packing) to
  `vendor/stb/` at pinned revisions, alongside the existing `stb_image.h`.
  fontstash is used only as a **design reference** for the atlas and
  line-breaking algorithms — it is not vendored, and the sokol port
  (`sokol_fontstash.h`) is rejected (see design).
- **New native resource: `FontData`.** `efx.loadFontData(path)` reads a
  `.ttf`/`.otf` from the resource root and returns an opaque, GC-finalized
  `FontData` (parsed font, no GPU resource). `destroy()` releases it.
- **New native resource: `Font`.** `efx.createFont(fontData, opts)` bakes a
  **fixed** RGBA8 glyph atlas at creation and returns an opaque `Font`.
  `opts` carries `size` (required), `glyphs` (charset; defaults to the
  printable **Latin-1** set), `padding`, `filter`, and the baked **effects**
  `outline` (`{ width }`) and `shadow` (`{ blur, offset }`). The atlas is
  never grown after creation. `destroy()` releases the atlas.
- **Native typesetting/drawing: `efx.drawText(text, font, x, y, opts?)`.**
  C-implemented, 2D-only, opaque: it lays out the string and emits glyph
  quads into the display list. Formatting in scope is **word wrapping**,
  horizontal **left / center / right / justify**, and vertical
  **top / middle / bottom** — **no rich text** (no per-span styles/markup).
  Baked outline/shadow draw as ordered layers (shadow → outline → fill).
  Returns the laid-out bounds `{ width, height, lines }`.
- **Measurement: `efx.measureText(text, font, opts?)`.** Returns the same
  `{ width, height, lines }` bounds without drawing.
- **Taxonomy + docs.** `FontData` and `Font` join the native-backed class
  list; `docs/js-api.md`, `gallery/src/api/efx.d.ts`, and the type-test are
  updated in the same change. The F8 provisional `loadFont`/`drawText`
  entries are replaced by the settled contract.
- **Gallery showcase.** A curated `text-showcase` sample (font asset pack +
  manifest + CREDITS) demonstrating alignment, wrapping/justify, and baked
  outline/shadow, wired through the existing gallery build.
- **Verification.** Golden scenes for text rendering (display targets) plus
  headless unit tests for layout/measure/bounds (all targets).

## Capabilities

### New Capabilities

- `font-text`: font data loading, fixed atlas baking (size/charset/effects),
  C-side typesetting (wrap + alignment + justify + vertical alignment),
  opaque 2D text drawing as display-list quads, measurement, and the
  deterministic atlas/layout contract.

### Modified Capabilities

- `js-api`: adds the font/text script entries (`loadFontData`, `createFont`,
  `drawText`, `measureText`) and extends the native-backed resource class
  list with `FontData` and `Font` (including their documented query
  properties and `destroy()` lifecycle); the two-layer layering requirement
  is clarified so that text drawing is a mid-level C facility while the
  pure-JS high-level layer (e.g. `drawModel`) is unchanged.

## Impact

- **Dependencies (evaluated here):** `stb_truetype.h` (public domain / MIT)
  and `stb_rect_pack.h` (public domain), pinned snapshots, same family as
  the already-vendored `stb_image.h`; C11, no build system, works on
  Windows/Linux/macOS/Emscripten. `fontstash` (zlib) and
  `sokol_fontstash.h` (needs `sokol_gl.h`) are evaluated and rejected as
  vendored dependencies (design records the rationale). No other new
  dependencies.
- **Code:** new `src/render/text.[ch]` (pure C: parse, bake, pack, layout,
  draw) plus `src/render/stb_truetype_impl.c` /
  `src/render/stb_rect_pack_impl.c`; `src/render/render.h` (atlas texture
  creation via the existing texture path); `src/api/api.c` (two classes +
  four functions), `src/web/bridge.c`, `src/web/entry.js` (binding parity);
  `CMakeLists.txt` (vendor headers, impl TUs).
- **Docs:** `docs/js-api.md` (F8 section rewritten to current; resource
  table + limits note), `gallery/src/api/efx.d.ts` +
  `gallery/src/api/efx.type-test.ts`, a new ADR
  (`docs/decisions/0038-native-font-typesetting.md`) with its
  `docs/decisions/README.md` index row, `AGENTS.md` current-state + roadmap,
  `vision.md` (drop `drawText` from the pure-JS example list), and
  `vendor/README.md` (pin table rows).
- **Tests/samples:** golden scenes under `tests/goldens/` with a committed
  CC0 font; headless layout/measure unit tests; a curated
  `gallery/samples/curated/text-showcase.js` + `text-showcase.zip` +
  manifest + `CREDITS.md`; the desktop gallery smoke case.
- **Milestone:** F8 (text slice). Predecessors F7 and F9/F10 are green.
- **Verification:** ctest unit/layout tests on all four targets; golden-image
  scenes on the display-bearing targets; the Emscripten web suite + browser
  harness; the four-target gate before archive.

**Non-goals (out of scope):**

- **Rich text** — no per-span styles, inline colors/sizes, bold/italic
  synthesis, markup, or BiDi/complex-script shaping (HarfBuzz-class).
- **Dynamic atlas** — the atlas is fixed at `createFont`; no glyph-on-demand
  growth, no re-bake, no multi-size atlas in one `Font`.
- **3D text** — no world-space/billboard text; text is 2D quads in the
  current 2D frame only.
- **MSDF/SDF atlases**, variable fonts, hinting, subpixel/ClearType AA, or
  ligatures.
- **`drawModel` and the demo resource pack** — the remaining F8 slices.
- **Exposing glyph metrics or the atlas** to scripts (drawing stays opaque).
