# Spec Delta

## MODIFIED Requirements

### Requirement: 2D projection frame and camera
`setCamera2D(opts)` SHALL establish both the 2D projection frame and the
view transform. The frame is a virtual pixel size (`frame: [width, height]`);
every 2D draw coordinate and size SHALL be expressed in frame pixels. The
frame SHALL map onto the active rendering surface with a stretch policy: the
frame's full area fills the surface's full area, scaling non-uniformly if
the aspect ratios differ; no letterboxing or cropping. Frame origin SHALL be
the top-left corner with y pointing down, and 2D rotation angles SHALL be
degrees measured clockwise in this frame. The view fields `x` and `y` SHALL
name the world point displayed at the frame's center, and `zoom` and
`rotation` SHALL transform around that center: zoom 2 displays exactly half
the frame's world extent, still centered on `x`/`y`. When the script never
calls `setCamera2D`, the default camera SHALL have a frame equal to the
current rendering surface's size — the window, or the active
RenderTarget's extent while a begin/end pair is recording (pixel
coordinates match surface pixels; after a window resize, the default frame
follows the new size). Camera state SHALL apply to draws recorded after
the call; a recorded draw MUST NOT observe camera changes recorded later.

#### Scenario: Virtual frame decouples drawing from window size
- **WHEN** a script sets `frame: [640, 480]` and draws content in frame coordinates, and the window is resized to a different size and aspect
- **THEN** the same coordinates produce the same relative layout, stretched to fill the whole window

#### Scenario: Zoom pivots on the frame center
- **WHEN** the view is at `x`/`y` and zoom changes from 1 to 2
- **THEN** the world point at the frame center stays fixed at the center and the visible world extent halves

#### Scenario: Rotation pivots on the frame center
- **WHEN** the view is at `x`/`y` and rotation is set to 90 (degrees)
- **THEN** the world rotates 90° clockwise around the screen position of `x`/`y`, which remains at the frame center

#### Scenario: Default camera matches the window
- **WHEN** a script draws at pixel coordinates without ever calling `setCamera2D`, on the window and inside an active render target
- **THEN** the drawing appears at the same position relative to the current rendering surface (the window, following resizes; or the target, whose extent plays the window's role), pixel-for-pixel

#### Scenario: Camera is value-snapshotted per record
- **WHEN** a script records a quad, then calls `setCamera2D` with a different view, then records another quad
- **THEN** playback renders each quad with the camera state at its record time

### Requirement: Quad drawing
`drawQuad(x, y, texture, opts?)` SHALL record one textured quad. `x` and `y`
SHALL place the quad's top-left corner in frame pixels. The `texture`
argument SHALL be required and MUST be a live Texture or a live
RenderTarget (F5a) — passing nothing, a value that is neither, or a
destroyed Texture or RenderTarget SHALL throw `TypeError`.

The quad's size SHALL be determined in frame pixels by the first match of:
`opts.size` — a `[width, height]` array of finite numbers, both required and
> 0; else the drawn texture region's extent when `opts.sourceRect` is given;
else the texture's pixel size (for a RenderTarget, its `width`/`height`). A
`size` entry that is non-numeric, infinite, or ≤ 0 SHALL throw (`TypeError` /
`RangeError` respectively) and record nothing. A `sourceRect` of zero extent
in either dimension SHALL throw `RangeError` and record nothing. `opts.scale`
SHALL apply after the size is determined: size is pre-scale frame pixels and
scale multiplies the drawn extents.

Options: `color` (tint `[r, g, b, a]`, default opaque white), `rotation`
(degrees clockwise, default 0), `scale` (uniform positive factor, default 1;
non-finite SHALL throw `TypeError`, ≤ 0 SHALL throw `RangeError`),
`sourceRect` (the texture region drawn, `{ x, y, w, h }` in texture pixels —
for a RenderTarget, target pixels — default the full texture; a region
extending outside the texture bounds SHALL throw `RangeError`), and `origin`
(a `[px, py]` array of finite numbers — the pivot point for rotation and
scale, expressed in quad-local frame pixels relative to the quad's top-left;
default the determined size's center). The origin offset SHALL NOT itself be
rotated or scaled, and an unrotated, unscaled quad SHALL place its top-left
corner at `(x, y)` regardless of `origin`. Unknown or wrongly-typed option
fields SHALL throw `TypeError`.

#### Scenario: Textured quad with defaults
- **WHEN** a script calls `drawQuad(0, 0, tex)` for a 128×128 texture with known pixel colors
- **THEN** the full texture is drawn 1:1, tinted white, into the 128×128 frame area at the requested position

#### Scenario: Size derives from the source rect
- **WHEN** `drawQuad(40, 40, tex, { sourceRect: { x: 0, y: 0, w: 64, h: 32 } })` is called on a 128×128 texture
- **THEN** the named region is drawn 1:1 into a 64×32 frame area at the requested position

#### Scenario: Explicit size overrides derivation
- **WHEN** `opts.size` names a different area than the source region or texture size, e.g. a 64×32 source region drawn with `size: [128, 128]`
- **THEN** the region is stretched to fill the named size, overriding the source-rect and texture-size derivation

#### Scenario: Scale applies after size determination
- **WHEN** a 64×64 texture is drawn with `size: [32, 16]` and `scale: 2`
- **THEN** the drawn quad covers a 64×32 frame area, pivoting on its center

#### Scenario: Source rect selects a texture region
- **WHEN** `sourceRect` names the top-left quarter of the texture
- **THEN** only that region is sampled and drawn, stretched to the destination size

#### Scenario: Out-of-bounds source rect throws
- **WHEN** `sourceRect` extends beyond the texture's pixel dimensions
- **THEN** the call throws `RangeError` and records nothing

#### Scenario: Zero-extent source rect throws
- **WHEN** `sourceRect` has `w` or `h` equal to 0
- **THEN** the call throws `RangeError` and records nothing

#### Scenario: Rotation and scale pivot on the quad center
- **WHEN** a quad is drawn with rotation 90 and scale 2 and no `origin`
- **THEN** the quad's center stays at the center of the determined size's rectangle at `x, y`, and the quad rotates clockwise and doubles in size around it

#### Scenario: Origin moves the pivot
- **WHEN** a quad is drawn with `origin: [0, 0]` and rotation 90
- **THEN** the quad rotates clockwise around its top-left corner, which stays fixed at `(x, y)`

#### Scenario: Origin does not move an untransformed quad
- **WHEN** two identical quads are drawn without rotation or scale, one with and one without `origin`
- **THEN** both render pixel-identically with their top-left corner at `(x, y)`

#### Scenario: Invalid size or origin values throw
- **WHEN** `drawQuad` is called with `size: [0, 10]`, `size: [10]`, `size: 'big'`, or `origin: [NaN, 0]`
- **THEN** the call throws (`RangeError` for out-of-range numbers, `TypeError` for wrong types) and records nothing

#### Scenario: Missing or destroyed texture throws
- **WHEN** `drawQuad` is called with no texture, a value that is neither a Texture nor a RenderTarget, a destroyed Texture, or a destroyed RenderTarget
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Render target drawn like a texture
- **WHEN** a 512×512 RenderTarget rendered with known content is passed to `drawQuad(0, 0, rt, { sourceRect: { x: 0, y: 0, w: 256, h: 256 } })`
- **THEN** the top-left quarter of the target's content is drawn into a 256×256 frame area, exactly as the same call with a Texture would
