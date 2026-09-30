// Type definitions for the EmotionFX script-facing API.
//
// This is a LIVING DOCUMENT: it describes the current public `efx` surface
// and grows with it. Update this file in the same change as any script-facing
// API change, alongside docs/js-api.md (see AGENTS.md).
//
// Status: F1, F2, F3, F4a, F4b, F5a, F5b, F6a, F6b, F6c, F6e, F7, F8a, F9,
// F10, F11, F12, and F13 are current behavior. F6d (the `--repl [<root>]`
// interactive console) adds no API — it drives this same namespace from stdin;
// `.help`/`.exit` are host commands, not `efx` functions. F8 is complete (F8b —
// `drawModel`/demo pack — was retired as obsolete, superseded by multi-surface
// meshes).
//
// The declarations are global/ambient so they can be loaded verbatim into the
// gallery editor (Monaco `addExtraLib`) and type-checked by `tsc`.

/**
 * An RGBA color: four normalized floats in `0..1`, ordered `[r, g, b, a]`
 * (e.g. `[1, 0.5, 0, 1]`). Most lighting and material channels ignore the
 * alpha component.
 */
type Color = [number, number, number, number];

/** A 2-component vector `[x, y]` (frame pixels for 2D APIs, world units for 3D). */
type Vec2 = [number, number];

/** A 3-component vector `[x, y, z]` in world units. */
type Vec3 = [number, number, number];

/** A column-major 4×4 matrix as a flat 16-number array. */
type Mat4 = [
  number, number, number, number,
  number, number, number, number,
  number, number, number, number,
  number, number, number, number,
];

/** A quaternion `[x, y, z, w]`. */
type Quat = [number, number, number, number];

// ---------------------------------------------------------------------------
// F9 — input: keyboard, mouse & window
// ---------------------------------------------------------------------------

/** F9: engine-owned lowercase keyboard identifier set (docs/js-api.md). */
type EfxKey =
  | 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'g' | 'h' | 'i' | 'j' | 'k' | 'l'
  | 'm' | 'n' | 'o' | 'p' | 'q' | 'r' | 's' | 't' | 'u' | 'v' | 'w' | 'x'
  | 'y' | 'z'
  | '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
  | 'f1' | 'f2' | 'f3' | 'f4' | 'f5' | 'f6' | 'f7' | 'f8' | 'f9' | 'f10'
  | 'f11' | 'f12'
  | 'space' | 'apostrophe' | 'comma' | 'minus' | 'period' | 'slash'
  | 'semicolon' | 'equal' | 'leftbracket' | 'backslash' | 'rightbracket'
  | 'grave' | 'escape' | 'enter' | 'tab' | 'backspace' | 'insert' | 'delete'
  | 'right' | 'left' | 'down' | 'up' | 'pageup' | 'pagedown' | 'home' | 'end'
  | 'capslock' | 'scrolllock' | 'numlock' | 'printscreen' | 'pause'
  | 'kp0' | 'kp1' | 'kp2' | 'kp3' | 'kp4' | 'kp5' | 'kp6' | 'kp7' | 'kp8'
  | 'kp9' | 'kpdecimal' | 'kpdivide' | 'kpmultiply' | 'kpsubtract' | 'kpadd'
  | 'kpenter' | 'kpequal'
  | 'lshift' | 'lctrl' | 'lalt' | 'lsuper'
  | 'rshift' | 'rctrl' | 'ralt' | 'rsuper'
  | 'menu';

/** F9: engine-owned mouse button identifier set. */
type EfxMouseButton = 'left' | 'right' | 'middle';

/** F9: an active keyboard modifier name reported in event `mods`. */
type EfxMod = 'shift' | 'ctrl' | 'alt' | 'super';

/** F9: payload of a key-down event (`efx.keyboard.onDown`). */
interface KeyboardDownEvent {
  /** The key that went down. */
  key: EfxKey;
  /** True when this is an auto-repeat rather than the initial press. */
  repeat: boolean;
  /** Modifier keys held at the moment of the event. */
  mods: EfxMod[];
}

/** F9: payload of a key-up event (`efx.keyboard.onUp`). */
interface KeyboardUpEvent {
  /** The key that went up. */
  key: EfxKey;
  /** Modifier keys held at the moment of the event. */
  mods: EfxMod[];
}

/** F9: payload of a text-input event (`efx.keyboard.onChar`). */
interface CharEvent {
  /** The decoded character, e.g. `'A'` (may be more than one UTF-16 unit). */
  char: string;
}

/** F9: payload of a mouse button event (`efx.mouse.onDown` / `onUp`). */
interface MouseButtonEvent {
  /** The button that changed state. */
  button: EfxMouseButton;
  /** Cursor x in surface (framebuffer) pixels, top-left origin. */
  x: number;
  /** Cursor y in surface (framebuffer) pixels, top-left origin, y down. */
  y: number;
  /** Modifier keys held at the moment of the event. */
  mods: EfxMod[];
}

/** F9: payload of a mouse move event (`efx.mouse.onMove`). */
interface MouseMoveEvent {
  /** Cursor x in surface pixels. */
  x: number;
  /** Cursor y in surface pixels. */
  y: number;
  /** Horizontal movement since the previous frame, in surface pixels. */
  dx: number;
  /** Vertical movement since the previous frame, in surface pixels. */
  dy: number;
}

/** F9: payload of a mouse wheel event (`efx.mouse.onWheel`). */
interface MouseWheelEvent {
  /** Horizontal scroll delta for this frame. */
  dx: number;
  /** Vertical scroll delta for this frame. */
  dy: number;
}

/**
 * F9 keyboard namespace. Queries report current frame state; `isPressed` /
 * `isReleased` are one-frame edges. Event registrations return an idempotent
 * unsubscribe function.
 */
interface EfxKeyboard {
  /** True while `key` is held. Throws `TypeError` for an unknown key name. */
  isDown(key: EfxKey): boolean;
  /** True on the frame `key` transitioned down. Throws `TypeError` for an unknown key. */
  isPressed(key: EfxKey): boolean;
  /** True on the frame `key` transitioned up. Throws `TypeError` for an unknown key. */
  isReleased(key: EfxKey): boolean;
  /** Subscribe to key-down events; returns an unsubscribe function. */
  onDown(fn: (e: KeyboardDownEvent) => void): () => void;
  /** Subscribe to key-up events; returns an unsubscribe function. */
  onUp(fn: (e: KeyboardUpEvent) => void): () => void;
  /** Subscribe to text-input events; returns an unsubscribe function. */
  onChar(fn: (e: CharEvent) => void): () => void;
}

/**
 * F9 mouse namespace. Queries report current frame state; `isPressed` /
 * `isReleased` are one-frame edges. Event registrations return an idempotent
 * unsubscribe function.
 */
interface EfxMouse {
  /** True while `button` is held. Throws `TypeError` for an unknown button. */
  isDown(button: EfxMouseButton): boolean;
  /** True on the frame `button` transitioned down. */
  isPressed(button: EfxMouseButton): boolean;
  /** True on the frame `button` transitioned up. */
  isReleased(button: EfxMouseButton): boolean;
  /** Subscribe to button-down events; returns an unsubscribe function. */
  onDown(fn: (e: MouseButtonEvent) => void): () => void;
  /** Subscribe to button-up events; returns an unsubscribe function. */
  onUp(fn: (e: MouseButtonEvent) => void): () => void;
  /** Subscribe to move events; returns an unsubscribe function. */
  onMove(fn: (e: MouseMoveEvent) => void): () => void;
  /** Subscribe to wheel events; returns an unsubscribe function. */
  onWheel(fn: (e: MouseWheelEvent) => void): () => void;
  /** Cursor position `[x, y]` in surface pixels. */
  readonly position: Vec2;
  /** Cursor x in surface pixels. */
  readonly x: number;
  /** Cursor y in surface pixels. */
  readonly y: number;
  /** Cursor movement `[dx, dy]` for the current frame, in surface pixels. */
  readonly delta: Vec2;
  /** Wheel delta `[dx, dy]` for the current frame. */
  readonly wheel: Vec2;
}

/** F9 read-only window metrics, in surface (framebuffer) pixels. */
interface EfxWindow {
  /** Window size `[width, height]` in surface pixels. */
  readonly size: Vec2;
  /** Window width in surface pixels. */
  readonly width: number;
  /** Window height in surface pixels. */
  readonly height: number;
  /** Surface-to-logical ratio on high-DPI displays (1.0 otherwise). */
  readonly dpiScale: number;
}

// ---------------------------------------------------------------------------
// F13 — gamepad input
// ---------------------------------------------------------------------------

/** F13: engine-owned semantic gamepad button identifier set. */
type EfxGamepadButton =
  | 'south' | 'east' | 'west' | 'north'
  | 'leftShoulder' | 'rightShoulder' | 'leftTrigger' | 'rightTrigger'
  | 'back' | 'start' | 'guide' | 'leftStick' | 'rightStick'
  | 'dpadUp' | 'dpadDown' | 'dpadLeft' | 'dpadRight';

/** F13: engine-owned semantic gamepad axis identifier set. */
type EfxGamepadAxis =
  | 'leftX' | 'leftY' | 'rightX' | 'rightY' | 'leftTrigger' | 'rightTrigger';

/** F13: a pad slot view. Plain data plus query methods; not a resource. */
interface EfxGamepadView {
  /** Slot index this view reports (0-based). */
  readonly index: number;
  /** True while a pad occupies this slot. */
  readonly connected: boolean;
  /** Device name string reported by the platform. */
  readonly name: string;
  /** True when a semantic mapping was found (else only `rawButton`/`rawAxis`). */
  readonly mapped: boolean;
  /** True while `button` is held. Throws `TypeError` for an unknown button. */
  isDown(button: EfxGamepadButton): boolean;
  /** True on the frame `button` transitioned down. */
  isPressed(button: EfxGamepadButton): boolean;
  /** True on the frame `button` transitioned up. */
  isReleased(button: EfxGamepadButton): boolean;
  /** Normalized axis value: sticks `-1..1`, triggers `0..1`. */
  axis(axis: EfxGamepadAxis): number;
  /** Raw device button value by index (for unmapped pads). */
  rawButton(index: number): number;
  /** Raw device axis value by index (for unmapped pads). */
  rawAxis(index: number): number;
}

/** F13 gamepad namespace (a fixed engine-owned bank of four pad slots). */
interface EfxGamepad {
  /** Number of currently connected pads. */
  readonly count: number;
  /** Pad view for `index`, or `null` when the slot is empty. Throws `TypeError` for a non-numeric index. */
  get(index: number): EfxGamepadView | null;
  /** Subscribe to connect events; returns an unsubscribe function. */
  onConnect(fn: (pad: EfxGamepadView) => void): () => void;
  /** Subscribe to disconnect events; returns an unsubscribe function. */
  onDisconnect(fn: (pad: EfxGamepadView) => void): () => void;
}

// ---------------------------------------------------------------------------
// Resource types
// ---------------------------------------------------------------------------

/** F2: raw CPU pixels plus size and format (opaque native-backed class). */
interface EfxImageData {
  /** Image width in pixels. Throws `TypeError` when destroyed. */
  readonly width: number;
  /** Image height in pixels. Throws `TypeError` when destroyed. */
  readonly height: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** F2: a GPU texture (opaque native-backed class). */
interface EfxTexture {
  /** Texture width in pixels. Throws `TypeError` when destroyed. */
  readonly width: number;
  /** Texture height in pixels. Throws `TypeError` when destroyed. */
  readonly height: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** F5a: a GPU render target (color + depth; opaque native-backed class). */
interface EfxRenderTarget {
  /** Target width in pixels. Throws `TypeError` when destroyed. */
  readonly width: number;
  /** Target height in pixels. Throws `TypeError` when destroyed. */
  readonly height: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** A live Texture or RenderTarget — accepted anywhere a texture is sampled. */
type EfxSample = EfxTexture | EfxRenderTarget;

/** F3: CPU mesh data (1..16 surfaces; opaque native-backed class). */
interface EfxMeshData {
  /** Number of surfaces (1..16). */
  readonly surfaceCount: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** F3: a GPU mesh uploaded from MeshData (opaque native-backed class). */
interface EfxMesh {
  /** Number of surfaces (1..16). */
  readonly surfaceCount: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

// ---------------------------------------------------------------------------
// F8a — font + text
// ---------------------------------------------------------------------------

/** F8a: a parsed TrueType/OpenType font (CPU only; opaque native-backed class). */
interface EfxFontData {
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** F8a: a baked glyph atlas plus layout metrics (opaque native-backed class). */
interface EfxFont {
  /** Pixel size the atlas was baked at. */
  readonly size: number;
  /** Line advance in pixels for the baked size. */
  readonly lineHeight: number;
  /** Distance from the baseline to the top of the em box, in pixels. */
  readonly ascent: number;
  /** Distance from the baseline to the bottom of the em box, in pixels. */
  readonly descent: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** F8a: a baked outline ring around glyphs. */
interface FontOutline {
  /** Outline thickness in pixels (must be > 0). */
  width: number;
}

/** F8a: a baked blurred shadow behind glyphs. */
interface FontShadow {
  /** Blur radius in pixels (must be > 0). */
  blur: number;
  /** Pixel offset `[dx, dy]`; defaults to `[0, 0]`. */
  offset?: Vec2;
}

/** F8a: options for `createFont`. */
interface CreateFontOptions {
  /** Pixel size baked into the atlas (must be > 0). */
  size: number;
  /** Codepoints to bake; defaults to the printable Latin-1 set. */
  glyphs?: string;
  /** Atlas gutter in pixels (default 1). */
  padding?: number;
  /** Atlas sampler filter (default `'linear'`). */
  filter?: 'linear' | 'nearest';
  /** Baked outline ring; `null`/omitted bakes none. */
  outline?: FontOutline | null;
  /** Baked blurred shadow; `null`/omitted bakes none. */
  shadow?: FontShadow | null;
}

/** F8a: options for `drawText` / `measureText`. */
interface TextOptions {
  /** Horizontal alignment (default `'left'`); `'justify'` requires `width`. */
  align?: 'left' | 'center' | 'right' | 'justify';
  /** Vertical alignment relative to `y` (default `'top'`). */
  valign?: 'top' | 'middle' | 'bottom';
  /** Wrap width in pixels; required for `'justify'`. */
  width?: number;
  /** Line advance in pixels; defaults to the font's `lineHeight`. */
  lineHeight?: number;
  /** Fill color (default opaque white). */
  color?: Color;
  /** Baked-outline color (default black). */
  outlineColor?: Color;
  /** Baked-shadow color (default black). */
  shadowColor?: Color;
  /** Rotation in degrees about the anchor (default 0). */
  rotation?: number;
  /** Uniform scale (default 1). */
  scale?: number;
}

/** F8a: laid-out text bounds returned by `drawText` / `measureText`. */
interface TextBounds {
  /** Laid-out width in pixels. */
  readonly width: number;
  /** Laid-out height in pixels. */
  readonly height: number;
  /** Number of laid-out lines. */
  readonly lines: number;
}

// ---------------------------------------------------------------------------
// F2 — 2D drawing
// ---------------------------------------------------------------------------

/** F2: options for `drawQuad`. */
interface DrawQuadOptions {
  /** Tint `[r, g, b, a]` (default opaque white). */
  color?: Color;
  /** Rotation in degrees clockwise (default 0). */
  rotation?: number;
  /** Uniform scale factor applied after the size is determined (default 1, must be > 0). */
  scale?: number;
  /** Quad size `[width, height]` in frame pixels; defaults to the source rect or texture size. */
  size?: Vec2;
  /** Pivot `[px, py]` in quad-local pixels for rotation/scale (default the size's center). */
  origin?: Vec2;
  /** Texture-pixel region to sample; defaults to the full texture. */
  sourceRect?: SourceRect;
}

/** F2: options for `setCamera2D`. */
interface Camera2DOptions {
  /** Virtual resolution `[width, height]`; defaults to the current window size. */
  frame?: Vec2;
  /** World x shown at the frame center (default: frame center). */
  x?: number;
  /** World y shown at the frame center (default: frame center). */
  y?: number;
  /** Zoom factor around the frame center (default 1, must be > 0). */
  zoom?: number;
  /** Rotation in degrees around the frame center (default 0). */
  rotation?: number;
}

/** F2: options for `createImageData`. */
interface CreateImageDataOptions {
  /** Image width in pixels (must be > 0). */
  width: number;
  /** Image height in pixels (must be > 0). */
  height: number;
  /** Flat RGBA8 bytes of length `width * height * 4`. */
  pixels: number[] | Uint8Array;
  /** Pixel format; only `'rgba8'` is supported (default `'rgba8'`). */
  format?: 'rgba8';
}

/** F2/F6e: sampler options for `createTexture`. */
interface TextureOptions {
  /** Texture wrap mode (default `'repeat'`). */
  wrap?: 'repeat' | 'clamp' | 'mirror';
  /** Texture filter (default `'linear'`); also drives the mipmap filter. */
  filter?: 'linear' | 'nearest';
  /** Build and use a full mip chain (default `false`). */
  mipmaps?: boolean;
}

/** A `{ x, y, w, h }` rectangle in texture pixels. */
interface SourceRect {
  /** Left edge in texture pixels. */
  x: number;
  /** Top edge in texture pixels. */
  y: number;
  /** Width in texture pixels (must be > 0). */
  w: number;
  /** Height in texture pixels (must be > 0). */
  h: number;
}

// ---------------------------------------------------------------------------
// F3 — 3D core
// ---------------------------------------------------------------------------

/** F3: options for `setCamera3D`. */
interface Camera3DOptions {
  /** Camera position in world units. */
  pos: Vec3;
  /** Point the camera looks at, in world units. */
  target: Vec3;
  /** Vertical field of view in degrees. */
  fov: number;
  /** Near plane distance (default 0.1). */
  near?: number;
  /** Far plane distance (default 100). */
  far?: number;
}

/** Flat floating-point attribute arrays: a plain array or a typed array. */
type FlatNumbers = number[] | Float32Array;

/** Flat index arrays: a plain array or a typed array of unsigned integers. */
type FlatIndices = number[] | Uint32Array;

/** F3/F6c: one mesh surface's attribute arrays (a Godot surface / glTF primitive). */
interface MeshSurfaceData {
  /** Required flat xyz positions (3 numbers per vertex). */
  positions: FlatNumbers;
  /** Optional flat normals (3 numbers per vertex); defaults to `(0,0,1)`. */
  normals?: FlatNumbers;
  /** Optional flat texture coordinates (2 numbers per vertex). */
  uvs?: FlatNumbers;
  /** Optional flat vertex colors (4 numbers per vertex). */
  colors?: FlatNumbers;
  /** Four joint indices per vertex (glTF `JOINTS_0`); requires `weights`. */
  joints?: FlatIndices;
  /** Four joint weights per vertex (glTF `WEIGHTS_0`); requires `joints`. */
  weights?: FlatNumbers;
  /** Triangle-list indices; omitted means non-indexed (vertex count divisible by 3). */
  indices?: FlatIndices;
}

/** F4a/F4b: an ambient/diffuse/emissive Phong channel. */
interface PhongChannel {
  /** Channel color `[r, g, b, a]` (alpha ignored by shading). */
  color: Color;
  /** Optional modulating texture (or render target); `null`/omitted means none. */
  map?: EfxSample | null;
}

/** F4a/F4b: the specular Phong channel (adds a shininess exponent). */
interface SpecularChannel {
  /** Specular color `[r, g, b, a]` (alpha ignored by shading). */
  color: Color;
  /** Specular exponent (default 32). */
  shininess?: number;
  /** Optional modulating texture (or render target); `null`/omitted means none. */
  map?: EfxSample | null;
}

/** F4a/F4b: a per-surface Phong material (JS-managed, snapshotted at binding). */
interface Material {
  /** Ambient channel (default black). */
  ambient?: PhongChannel;
  /** Diffuse channel (default white). */
  diffuse?: PhongChannel;
  /** Specular channel (default black, shininess 32). */
  specular?: SpecularChannel;
  /** Emissive channel (default black). */
  emissive?: PhongChannel;
  /** Material-level binary cutout; fragments sampling alpha < 0.5 are discarded. */
  alphaMask?: EfxSample | null;
}

/** F3: the multi-surface batch form of `createMeshData`. */
interface MeshDataBatch {
  /** 1..16 surfaces, each a Godot surface / glTF primitive. */
  surfaces: MeshSurfaceData[];
  /** One entry per surface; `null` selects the engine default material. */
  materials?: (Material | null)[];
  /** Shorthand fields are forbidden in the batch form (exclusive union). */
  positions?: never;
  normals?: never;
  uvs?: never;
  colors?: never;
  joints?: never;
  weights?: never;
  indices?: never;
}

/** F3: the single-surface shorthand form of `createMeshData`. */
interface MeshDataShorthand extends MeshSurfaceData {
  /** One entry; `null` selects the engine default material. */
  materials?: (Material | null)[];
  /** The batch field is forbidden in the shorthand form (exclusive union). */
  surfaces?: never;
}

/** F3: `createMeshData` accepts either the batch or the shorthand form. */
type CreateMeshDataOptions = MeshDataBatch | MeshDataShorthand;

/** F3/F7: options for `drawMesh`. */
interface DrawMeshCallOptions {
  /** Column-major transform (default identity). */
  transform?: Mat4;
  /** Tint multiplying vertex colors (default opaque white). */
  color?: Color;
  /** F7: `true` draws the current CPU-posed vertices; absent/false the bind pose. */
  skinned?: boolean;
}

/** F3: options for `makeCube`. */
interface MakeCubeOptions {
  /** Edge length (default 1, must be > 0). */
  size?: number;
  /** Material bound to the primitive's single surface; `null` = engine default. */
  material?: Material | null;
}

/** F3: options for `makePlane`. */
interface MakePlaneOptions {
  /** Edge length (default 1, must be > 0). */
  size?: number;
  /** Grid subdivisions per side (positive integer, default 1). */
  segments?: number;
  /** Material bound to the primitive's single surface; `null` = engine default. */
  material?: Material | null;
}

/** F3: options for `makeSphere`. */
interface MakeSphereOptions {
  /** Sphere radius (default 1, must be > 0). */
  radius?: number;
  /** Longitude/latitude subdivisions (positive integer, default 16). */
  segments?: number;
  /** Material bound to the primitive's single surface; `null` = engine default. */
  material?: Material | null;
}

/** F3: options for `makeCapsule`. */
interface MakeCapsuleOptions {
  /** Capsule radius (default 1, must be > 0). */
  radius?: number;
  /** Total tip-to-tip length including caps; must be >= 2 * radius (default 2). */
  height?: number;
  /** Longitude/latitude subdivisions (positive integer, default 16). */
  segments?: number;
  /** Material bound to the primitive's single surface; `null` = engine default. */
  material?: Material | null;
}

// ---------------------------------------------------------------------------
// F4a — lights
// ---------------------------------------------------------------------------

/** F4a: options for `setLight` (a point light). */
interface PointLightOptions {
  /** Light position in world units. */
  pos: Vec3;
  /** Light color `[r, g, b, a]` (alpha ignored). */
  color: Color;
  /** Attenuation radius (finite, >= 0; default 0 = no falloff). */
  range?: number;
}

/** F4a: options for `setDirectionalLight`. */
interface DirectionalLightOptions {
  /** Direction the light travels (the direction to the light is `-dir`). */
  dir: Vec3;
  /** Light color `[r, g, b, a]` (alpha ignored). */
  color: Color;
}

// ---------------------------------------------------------------------------
// F5a — render targets
// ---------------------------------------------------------------------------

/** F5a: options for `createRenderTarget`. */
interface RenderTargetOptions {
  /** Target width in pixels (positive integer, 1..4096). */
  width: number;
  /** Target height in pixels (positive integer, 1..4096). */
  height: number;
}

// ---------------------------------------------------------------------------
// F5b — post effects & render scale
// ---------------------------------------------------------------------------

/** F5b: a color-filter post effect (identity with all defaults). */
interface ColorFilterPostEffect {
  /** Discriminator selecting the color-filter effect. */
  effect: 'colorFilter';
  /** Input/output blend in `0..1` (default 1). */
  mix?: number;
  /** Multiplies color (finite, >= 0; default 1). */
  brightness?: number;
  /** Contrast pivoting at 0.5 grey (finite, >= 0; default 1). */
  contrast?: number;
  /** Saturation; 0 is fully grey, 1 is unchanged (finite, >= 0; default 1). */
  saturation?: number;
  /** RGB multiplier (alpha ignored); four finite components in `0..1`. */
  tint?: Color;
}

/** F5b: a separable gaussian blur post effect. */
interface BlurPostEffect {
  /** Discriminator selecting the blur effect. */
  effect: 'blur';
  /** Input/output blend in `0..1` (default 1). */
  mix?: number;
  /** Blur radius in scene pixels (finite, > 0 and <= 64; default 1). */
  radius?: number;
}

/** F5b: a bloom post effect. */
interface BloomPostEffect {
  /** Discriminator selecting the bloom effect. */
  effect: 'bloom';
  /** Input/output blend in `0..1` (default 1). */
  mix?: number;
  /** Luminance below which texels contribute nothing (finite, `0..1`; default 0.8). */
  threshold?: number;
  /** Additive contribution of the bloom (finite, `0..1`; default 0.5). */
  strength?: number;
}

/** F5b: one entry of the declarative post-effect chain (`setPostEffects`). */
type EfxPostEffect = ColorFilterPostEffect | BlurPostEffect | BloomPostEffect;

/** F5b: options for `setRenderScale`. */
interface RenderScaleOptions {
  /** Final blit filter (default `'linear'`). */
  filter?: 'nearest' | 'linear';
}

// ---------------------------------------------------------------------------
// F6 — resource loading
// ---------------------------------------------------------------------------

/** F6b: options for `loadMeshData`. */
interface LoadMeshDataOptions {
  /** Mesh selector: a non-negative index or a mesh name; defaults to the first mesh. */
  mesh?: number | string;
}

// ---------------------------------------------------------------------------
// F7 — skinning & animation
// ---------------------------------------------------------------------------

/**
 * F7 pose sample: one clip sampled at `time` (seconds) with an optional blend
 * `weight` (normalized engine-side across an array; a single sample ignores
 * it). `clip` is a clip name or an index.
 */
interface PoseSample {
  /** Clip name (glTF `name`, or stable `clipN`) or a non-negative clip index. */
  clip: string | number;
  /** Sample time in seconds; wraps modulo the clip length. */
  time: number;
  /** Blend weight (>= 0); normalized across an array, ignored for a single sample. */
  weight?: number;
}

// ---------------------------------------------------------------------------
// F11 — billboards, 2D sprites & CPU particles
// ---------------------------------------------------------------------------

/** F11: quad render mode for world-space billboards and particles. */
type EfxFacing = 'view' | 'y' | 'plane';

/** F2/F11: blend mode for 2D quads, sprites, and particle batches. */
type EfxBlendMode = 'alpha' | 'additive' | 'subtractive';

/** F11: options for `drawBillboard`. */
interface DrawBillboardOptions {
  /** Texture (or render target) to draw; required. */
  texture: EfxSample;
  /** World-unit size: a single number or `[w, h]` (default 1). */
  size?: number | Vec2;
  /** Tint `[r, g, b, a]` (default opaque white). */
  color?: Color;
  /** Texture-pixel atlas region; defaults to the full texture. */
  sourceRect?: SourceRect;
  /** In-plane rotation in degrees (default 0). */
  rotation?: number;
  /** `'view'` (default, full camera-facing), `'y'` (world-up), or `'plane'` (fixed oriented plane). */
  facing?: EfxFacing;
  /** Plane orientation normal for `facing: 'plane'` (default `[0, 1, 0]`). */
  normal?: Vec3;
  /** Depth-test against opaque geometry (default `true`); never writes depth. */
  depthTest?: boolean;
}

/** F11: one entry of a `drawSprites` batch (equivalent to a `drawQuad` call). */
interface SpriteOptions {
  /** Quad top-left x in frame pixels. */
  x: number;
  /** Quad top-left y in frame pixels. */
  y: number;
  /** Quad size `[width, height]` in frame pixels; defaults to the source rect or texture size. */
  size?: Vec2;
  /** Tint `[r, g, b, a]` (default opaque white). */
  color?: Color;
  /** Rotation in degrees clockwise (default 0). */
  rotation?: number;
  /** Uniform scale factor (default 1, must be > 0). */
  scale?: number;
  /** Texture-pixel region to sample; defaults to the full texture. */
  sourceRect?: SourceRect;
  /** Pivot `[px, py]` in quad-local pixels for rotation/scale (default the size's center). */
  origin?: Vec2;
}

/** F11: emission volume for a particle system. */
interface EmissionShapeOptions {
  /** Shape kind. */
  shape: 'point' | 'box' | 'sphere' | 'sphereSurface' | 'disc';
  /** Shape extent `[x, y, z]` (default `[0, 0, 0]`). */
  size?: Vec3;
}

/** F11: options for `createParticleSystem` (`texture`, `max`, `lifetime` required). */
interface ParticleSystemOptions {
  /** Texture (or render target) for particle quads; required. */
  texture: EfxSample;
  /** Maximum live particles (integer, 1..65536); required. */
  max: number;
  /** Simulation space: `'world'` (default, 3D) or `'screen'` (2D). */
  space?: 'world' | 'screen';
  /** Quad render mode in world space (default `'view'`; screen space must be `'view'`). */
  facing?: EfxFacing;
  /** Plane orientation normal for `facing: 'plane'` (default `[0, 1, 0]`). */
  normal?: Vec3;
  /** Blend mode (default `'alpha'`). */
  blend?: EfxBlendMode;
  /** Particle lifetime in seconds: a number or `[min, max]`; required. */
  lifetime: number | [number, number];
  /** Particles emitted per second (default 0). */
  emissionRate?: number;
  /** Emitter lifetime in seconds; `-1` is infinite. */
  emitterLifetime?: number;
  /** Spawn position (2- or 3-component vector). */
  position?: Vec2 | Vec3;
  /** Emission direction (2- or 3-component vector). */
  direction?: Vec2 | Vec3;
  /** Emission cone half-angle in degrees. */
  spread?: number;
  /** Initial speed: a number or `[min, max]`. */
  speed?: number | [number, number];
  /** Constant acceleration (2- or 3-component vector). */
  gravity?: Vec2 | Vec3;
  /** Additional constant acceleration (2- or 3-component vector). */
  linearAcceleration?: Vec2 | Vec3;
  /** Radial acceleration: a number or `[min, max]`. */
  radialAcceleration?: number | [number, number];
  /** Tangential acceleration: a number or `[min, max]`. */
  tangentialAcceleration?: number | [number, number];
  /** Linear damping: a number or `[min, max]`. */
  linearDamping?: number | [number, number];
  /** Size over the lifetime: one number or up to 8 interpolated keyframes. */
  sizes?: number | number[];
  /** Per-particle size variation (`0..1`). */
  sizeVariation?: number;
  /** Color over the lifetime: one color or up to 8 interpolated keyframes. */
  colors?: Color | Color[];
  /** Rotation in degrees: a number or `[min, max]`. */
  rotation?: number | [number, number];
  /** Angular velocity in degrees/second: a number or `[min, max]`. */
  spin?: number | [number, number];
  /** Per-particle spin variation. */
  spinVariation?: number;
  /** When true, particle angle follows its velocity. */
  relativeRotation?: boolean;
  /** Emission volume; defaults to a point. */
  emissionShape?: EmissionShapeOptions;
  /** Atlas frames cycled over the lifetime. */
  quads?: SourceRect[];
  /** Draw order within the batch (default `'top'`). */
  insertMode?: 'top' | 'bottom' | 'random';
  /** Simulated-time factor (default 1). */
  speedScale?: number;
}

/** F11: partial update bag for `ParticleSystem.set`. */
type ParticleSystemSetOptions = Partial<ParticleSystemOptions>;

/** F11: a native-backed CPU particle system. */
interface EfxParticleSystem {
  /** Emit `n` particles immediately (a burst). */
  emit(n: number): void;
  /** Start continuous emission. */
  start(): void;
  /** Stop emitting (live particles keep simulating). */
  stop(): void;
  /** Pause simulation. */
  pause(): void;
  /** Reset the system to its initial state. */
  reset(): void;
  /** Apply a partial options update atomically. */
  set(opts: ParticleSystemSetOptions): void;
  /** Number of live particles. */
  readonly count: number;
  /** Read-write simulated-time factor. */
  speedScale: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

// ---------------------------------------------------------------------------
// F3 — pure-JS math layer
// ---------------------------------------------------------------------------

/** F3: pure-JS 4×4 matrix helpers (inputs are never mutated). */
interface EfxMat4 {
  /** Return the identity matrix. */
  identity(): Mat4;
  /** Build a perspective projection (column-major, GL convention). */
  perspective(fovY: number, aspect: number, near: number, far: number): Mat4;
  /** Build an orthographic projection. */
  ortho(w: number, h: number, near: number, far: number): Mat4;
  /** Return `m` translated by `v`. */
  translate(m: Mat4, v: Vec3): Mat4;
  /** Return `m` rotated `deg` degrees about `axis`. */
  rotate(m: Mat4, deg: number, axis: Vec3): Mat4;
  /** Return `m` scaled by `v`. */
  scale(m: Mat4, v: Vec3): Mat4;
  /** Return the matrix product `a · b` (b applies to a vector first). */
  multiply(a: Mat4, b: Mat4): Mat4;
}

/** F3: pure-JS 3-component vector helpers (inputs are never mutated). */
interface EfxVec3 {
  /** Return `a + b`. */
  add(a: Vec3, b: Vec3): Vec3;
  /** Return `a - b`. */
  sub(a: Vec3, b: Vec3): Vec3;
  /** Return `v * s`. */
  scale(v: Vec3, s: number): Vec3;
  /** Return the unit vector along `v`. */
  normalize(v: Vec3): Vec3;
  /** Return the cross product `a × b`. */
  cross(a: Vec3, b: Vec3): Vec3;
  /** Return the dot product `a · b`. */
  dot(a: Vec3, b: Vec3): number;
}

/** F3: pure-JS quaternion helpers (inputs are never mutated). */
interface EfxQuat {
  /** Return the identity quaternion. */
  identity(): Quat;
  /** Build a quaternion from an axis and an angle in degrees. */
  fromAxisAngle(deg: number, axis: Vec3): Quat;
  /** Return the quaternion product `a · b`. */
  multiply(a: Quat, b: Quat): Quat;
  /** Convert a quaternion to a rotation matrix. */
  toMat4(q: Quat): Mat4;
}

// ---------------------------------------------------------------------------
// F12 — collision, character & impulse dynamics
// ---------------------------------------------------------------------------

/** F12: a sphere collider. */
interface SphereShape {
  /** Discriminator selecting the sphere shape. */
  type: 'sphere';
  /** Sphere radius (must be > 0). */
  radius: number;
}

/** F12: an axis-aligned box collider. */
interface BoxShape {
  /** Discriminator selecting the box shape. */
  type: 'box';
  /** Full box extent `[x, y, z]` (each component > 0). */
  size: Vec3;
}

/** F12: a vertical capsule collider. */
interface CapsuleShape {
  /** Discriminator selecting the capsule shape. */
  type: 'capsule';
  /** Capsule radius (must be > 0). */
  radius: number;
  /** Total tip-to-tip height including caps; must be >= 2 * radius. */
  height: number;
}

/** F12: a static triangle-mesh collider built from a live Mesh. */
interface MeshShape {
  /** Discriminator selecting the triangle-mesh shape. */
  type: 'mesh';
  /** A live Mesh (static bodies and queries only). */
  mesh: EfxMesh;
}

/** F12: a collision shape accepted by bodies and by the spatial queries. */
type PhysicsShape = SphereShape | BoxShape | CapsuleShape | MeshShape;

/** F12: options for `physics.createBody`. */
interface CreateBodyOptions {
  /** Collider shape; required. */
  shape: PhysicsShape;
  /** Simulated by the solver when true (default `false` = static). */
  dynamic?: boolean;
  /** Report-only volume that never resolves (default `false`). */
  sensor?: boolean;
  /** Initial position in world units (default `[0, 0, 0]`). */
  position?: Vec3;
  /** Dynamic mass (default 1, must be positive). */
  mass?: number;
  /** Surface friction (default 0.5). */
  friction?: number;
  /** Bounciness in `[0, 1]` (default 0). */
  restitution?: number;
  /** Collision layer bitmask (32-bit; default all bits). */
  layer?: number;
  /** Collision mask bitmask (32-bit; default all bits). */
  mask?: number;
}

/** F12: options for `physics.createStaticMesh`. */
interface CreateStaticMeshOptions {
  /** Initial position in world units (default `[0, 0, 0]`). */
  position?: Vec3;
  /** Report-only volume that never resolves (default `false`). */
  sensor?: boolean;
  /** Surface friction (default 0.5). */
  friction?: number;
  /** Bounciness in `[0, 1]` (default 0). */
  restitution?: number;
  /** Collision layer bitmask (32-bit; default all bits). */
  layer?: number;
  /** Collision mask bitmask (32-bit; default all bits). */
  mask?: number;
}

/** F12: options for `physics.createCharacter`. */
interface CreateCharacterOptions {
  /** Capsule radius (must be > 0); required. */
  radius: number;
  /** Total tip-to-tip capsule height; must be >= 2 * radius; required. */
  height: number;
  /** Initial position in world units (default `[0, 0, 0]`). */
  position?: Vec3;
  /** Up direction (default `[0, 1, 0]`, must be non-zero). */
  up?: Vec3;
  /** Maximum walkable floor angle in degrees (default 45). */
  floorMaxAngle?: number;
  /** Floor snap distance (default 0.1). */
  floorSnapLength?: number;
  /** Step-up height; `0` disables step-up (default 0.3). */
  stepHeight?: number;
  /** Maximum slide iterations per move (positive integer, default 6). */
  maxSlides?: number;
  /** Collision safe margin (default 0.001). */
  safeMargin?: number;
  /** Collision layer bitmask (32-bit; default all bits). */
  layer?: number;
  /** Collision mask bitmask (32-bit; default all bits). */
  mask?: number;
}

/** F12: one contact reported on a dynamic body's `contacts` list. */
interface PhysicsContact {
  /** The other collider's handle (`null` for a static mesh or character). */
  readonly body: EfxBody | null;
  /** True when the contact is with a sensor. */
  readonly sensor: boolean;
  /** Contact normal `[x, y, z]` in world units. */
  readonly normal: Vec3;
  /** Contact point `[x, y, z]` in world units. */
  readonly point: Vec3;
  /** Penetration depth. */
  readonly depth: number;
  /** Applied impulse magnitude. */
  readonly impulse: number;
}

/** F12: a native-backed collider in the single physics world. */
interface EfxBody {
  /** Read-only world position (mutate `velocity` to move a dynamic body). */
  readonly position: Vec3;
  /** Read-write linear velocity. */
  velocity: Vec3;
  /** Read-only column-major translation matrix, usable directly by `drawMesh`. */
  readonly transform: Mat4;
  /** Read-only contacts from the last `step`; valid until the next `step`. */
  readonly contacts: PhysicsContact[];
  /** Apply an instantaneous impulse (dynamic only; else `TypeError`). */
  applyImpulse(v: Vec3): void;
  /** Apply a force for the next `step` (dynamic only; else `TypeError`). */
  applyForce(v: Vec3): void;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** F12: one collision reported by `Character.moveAndSlide`. */
interface PhysicsMoveCollision {
  /** The blocking collider's handle (`null` for a static mesh or character). */
  readonly body: EfxBody | null;
  /** Collision normal `[x, y, z]`. */
  readonly normal: Vec3;
  /** Collision point `[x, y, z]`. */
  readonly point: Vec3;
}

/** F12: the result of `Character.moveAndSlide`. */
interface PhysicsMoveResult {
  /** Resulting world position. */
  readonly position: Vec3;
  /** True when the move ended on a walkable floor. */
  readonly onFloor: boolean;
  /** True when the move was blocked by a wall. */
  readonly onWall: boolean;
  /** True when the move was blocked by a ceiling. */
  readonly onCeiling: boolean;
  /** Floor normal `[x, y, z]` when on a floor. */
  readonly floorNormal: Vec3;
  /** Collisions encountered during the move. */
  readonly collisions: PhysicsMoveCollision[];
}

/** F12: a native-backed kinematic capsule character controller. */
interface EfxCharacter {
  /** Read-only world position. */
  readonly position: Vec3;
  /** Read-write velocity (drives one-way pushes during `step`). */
  velocity: Vec3;
  /** Read-only: true when currently standing on a floor. */
  readonly onFloor: boolean;
  /** Sweep and slide the capsule by `motion` (world units). */
  moveAndSlide(motion: Vec3): PhysicsMoveResult;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** F12: options for `physics.raycast`. */
interface RaycastOptions {
  /** Maximum ray distance (positive finite); required. */
  maxDistance: number;
  /** Collision mask bitmask filter. */
  mask?: number;
  /** Return every hit sorted by distance instead of the first. */
  all?: boolean;
  /** Include sensors (excluded by default). */
  sensors?: boolean;
}

/** F12: one raycast hit. */
interface PhysicsRayHit {
  /** Hit point `[x, y, z]`. */
  readonly point: Vec3;
  /** Surface normal `[x, y, z]` at the hit. */
  readonly normal: Vec3;
  /** Distance from the ray origin. */
  readonly distance: number;
  /** The hit collider's handle (`null` for a static mesh or character). */
  readonly body: EfxBody | EfxCharacter | null;
}

/** F12: options for `physics.overlap`. */
interface OverlapOptions {
  /** Query position in world units (default `[0, 0, 0]`). */
  position?: Vec3;
  /** Collision mask bitmask filter. */
  mask?: number;
}

/** F12: options for `physics.shapeCast`. */
interface ShapeCastOptions {
  /** Collision mask bitmask filter. */
  mask?: number;
  /** Include sensors (excluded by default). */
  sensors?: boolean;
}

/** F12: one shape-cast hit. */
interface PhysicsShapeHit {
  /** Hit point `[x, y, z]`. */
  readonly point: Vec3;
  /** Surface normal `[x, y, z]` at the hit. */
  readonly normal: Vec3;
  /** Fraction along `motion` where the hit occurred (`0..1`). */
  readonly fraction: number;
  /** The hit collider's handle (`null` for a static mesh or character). */
  readonly body: EfxBody | EfxCharacter | null;
}

/** F12: the single physics world (script-stepped; the engine never steps). */
interface EfxPhysics {
  /** World gravity `[x, y, z]` (read-write; default `[0, -9.81, 0]`). */
  gravity: Vec3;
  /** Solver iteration count (read-write positive integer; default 8). */
  iterations: number;
  /** Advance the world by `dt` seconds. */
  step(dt: number): void;
  /** Remove every collider from the world. */
  clear(): void;
  /** Create a static, dynamic, or sensor body. */
  createBody(opts: CreateBodyOptions): EfxBody;
  /** Create a static triangle-mesh collider from a live Mesh. */
  createStaticMesh(mesh: EfxMesh, opts?: CreateStaticMeshOptions): EfxBody;
  /** Create a kinematic capsule character controller. */
  createCharacter(opts: CreateCharacterOptions): EfxCharacter;
  /** Cast a ray; returns the first hit, or `null` when nothing is hit. */
  raycast(origin: Vec3, direction: Vec3, opts: RaycastOptions): PhysicsRayHit | null;
  /** Cast a ray returning every hit sorted by distance (`opts.all: true`). */
  raycast(origin: Vec3, direction: Vec3, opts: RaycastOptions & { all: true }): PhysicsRayHit[];
  /** Return the live bodies/characters intersecting `shape` (including sensors). */
  overlap(shape: PhysicsShape, opts?: OverlapOptions): (EfxBody | EfxCharacter)[];
  /** Sweep `shape` from `from` by `motion`; returns the first hit, or `null`. */
  shapeCast(shape: PhysicsShape, from: Vec3, motion: Vec3,
            opts?: ShapeCastOptions): PhysicsShapeHit | null;
}

// ---------------------------------------------------------------------------
// The single `efx` namespace
// ---------------------------------------------------------------------------

/** The engine-provided script surface; the only global scripts use. */
interface Efx {
  // F1 — environment & lifecycle

  /** Print `msg` to stdout followed by a newline and flush. */
  log(msg?: unknown): void;
  /** Request engine termination with exit code `code` (default 0); never returns. */
  quit(code?: number): never;
  /** Return the host arguments passed after `--script <file>`. */
  args(): string[];
  /** Register a per-frame update hook `fn(dt)`; returns an unsubscribe function. */
  registerUpdateHook(fn: (dt: number) => void): () => void;
  /** Register a per-frame render hook `fn()`; returns an unsubscribe function. */
  registerRenderHook(fn: () => void): () => void;

  // F2 — 2D drawing

  /** Set the frame clear color `[r, g, b, a]` (default black). */
  setClearColor(color: Color): void;
  /** Set the 2D virtual pixel frame and its projection state. */
  setCamera2D(opts: Camera2DOptions): void;
  /** Build CPU pixels as an ImageData. */
  createImageData(opts: CreateImageDataOptions): EfxImageData;
  /** Upload ImageData to a GPU Texture with optional sampler settings. */
  createTexture(imageData: EfxImageData, opts?: TextureOptions): EfxTexture;
  /** Engine-owned 1×1 white Texture (read-only; `destroy()` throws). */
  readonly whiteTexture: EfxTexture;
  /** Record one textured quad at frame-pixel `(x, y)`. */
  drawQuad(x: number, y: number, texture: EfxSample, opts?: DrawQuadOptions): void;
  /** Set the blend mode for subsequently recorded 2D draws (default `'alpha'`). */
  setBlendMode(mode: EfxBlendMode): void;

  // F3 — 3D core

  /** Set the single 3D camera (separate from the 2D frame). */
  setCamera3D(opts: Camera3DOptions): void;
  /** Build multi-surface MeshData from the batch or shorthand form. */
  createMeshData(data: CreateMeshDataOptions): EfxMeshData;
  /** Upload all surfaces of MeshData to a GPU Mesh. */
  createMesh(meshData: EfxMeshData): EfxMesh;
  /** Draw a whole mesh, depth-tested, under the recorded 3D camera. */
  drawMesh(mesh: EfxMesh, opts?: DrawMeshCallOptions): void;
  /** Build single-surface cube MeshData. */
  makeCube(opts?: MakeCubeOptions): EfxMeshData;
  /** Build single-surface plane MeshData (on XZ, facing +Y). */
  makePlane(opts?: MakePlaneOptions): EfxMeshData;
  /** Build single-surface UV sphere MeshData. */
  makeSphere(opts?: MakeSphereOptions): EfxMeshData;
  /** Build single-surface vertical capsule MeshData. */
  makeCapsule(opts?: MakeCapsuleOptions): EfxMeshData;

  // F3 — pure-JS math layer

  /** Pure-JS 4×4 matrix helpers. */
  mat4: EfxMat4;
  /** Pure-JS 3-component vector helpers. */
  vec3: EfxVec3;
  /** Pure-JS quaternion helpers. */
  quat: EfxQuat;

  // F4a/F4b — lights & per-surface Phong materials

  /** Set point-light slot `0..3` (`null` disables the slot). */
  setLight(slot: number, opts: PointLightOptions | null): void;
  /** Set the single directional light (`null` disables it). */
  setDirectionalLight(opts: DirectionalLightOptions | null): void;
  /** Bind a Phong material to one mesh surface (`null` restores the default). */
  setMeshSurfaceMaterial(mesh: EfxMesh, surfaceIndex: number, mat: Material | null): void;

  // F5a — render targets

  /** Create a GPU render target with a color and depth attachment. */
  createRenderTarget(opts: RenderTargetOptions): EfxRenderTarget;
  /** Redirect subsequently recorded draws into `rt` (clears it on entry). */
  beginRenderTarget(rt: EfxRenderTarget): void;
  /** Return to the default target. */
  endRenderTarget(): void;

  // F5b — post effects & render scale

  /** Set the declarative post-effect chain (`null`/`[]` clears it; max 8 entries). */
  setPostEffects(list: EfxPostEffect[] | null): void;
  /** Set the scene-resolution scale and final blit filter. */
  setRenderScale(scale: number, opts?: RenderScaleOptions): void;

  // F6a — resource loading (paths relative to the resource root)

  /** Read a UTF-8 text resource. */
  loadText(path: string): string;
  /** Decode a PNG/JPEG image resource to RGBA8 ImageData. */
  loadImage(path: string): EfxImageData;

  // F6b — glTF 2.0 static import

  /** Import a glTF/GLB mesh (materials converted and bound per surface). */
  loadMeshData(path: string, opts?: LoadMeshDataOptions): EfxMeshData;

  // F8a — font + text (C-implemented mid-level facilities)

  /** Parse a `.ttf`/`.otf` font into FontData. */
  loadFontData(path: string): EfxFontData;
  /** Bake a fixed glyph atlas Font from FontData. */
  createFont(fontData: EfxFontData, opts: CreateFontOptions): EfxFont;
  /** Lay out and record 2D text quads; returns the laid-out bounds. */
  drawText(text: string, font: EfxFont, x: number, y: number,
           opts?: TextOptions): TextBounds;
  /** Lay out text without drawing; returns the same bounds. */
  measureText(text: string, font: EfxFont, opts?: TextOptions): TextBounds;

  // F7 — CPU skinning & animation (the script owns the clock)

  /** CPU-pose a skinned mesh in place from one sample or a weighted array. */
  poseMesh(mesh: EfxMesh, pose: PoseSample | PoseSample[]): void;

  // F9 — input: sub-namespaces of the single efx object

  /** Keyboard queries and events. */
  keyboard: EfxKeyboard;
  /** Mouse queries and events. */
  mouse: EfxMouse;
  /** Read-only window metrics. */
  window: EfxWindow;
  // F13 — gamepad input: fixed bank of pad slots
  /** Gamepad bank queries and connect/disconnect events. */
  gamepad: EfxGamepad;

  // F11 — world-space billboards, batched 2D sprites, CPU particles

  /** Record one world-space billboard quad at `pos`. */
  drawBillboard(pos: Vec3, opts: DrawBillboardOptions): void;
  /** Record a batch of 2D sprite quads from one texture. */
  drawSprites(texture: EfxSample, sprites: SpriteOptions[]): void;
  /** Create a native-backed CPU particle system. */
  createParticleSystem(opts: ParticleSystemOptions): EfxParticleSystem;
  /** Record one batch for a particle system's live particles. */
  drawParticles(system: EfxParticleSystem): void;

  // F12 — collision, character & impulse dynamics (single world, script-stepped)

  /** The single physics world. */
  physics: EfxPhysics;
}

declare const efx: Efx;

// ---------------------------------------------------------------------------
// F10 — CommonJS module authoring facilities
// ---------------------------------------------------------------------------
//
// Every script file under the resource root is a module; `require`/`module`/
// `exports` exist only inside a module's own scope (never on `efx` and never as
// true globals), and `require` loads synchronously from the resource root.
// TypeScript authors normally write `import`/`export` and let `tsc`
// (`module: commonjs`) emit the `require` form.

/** F10: one entry of `require.cache`. */
interface EfxModuleCacheEntry {
  /** Root-relative resolved module path. */
  id: string;
  /** The module's `exports` value. */
  exports: unknown;
  /** True once the module body has finished evaluating. */
  loaded: boolean;
}

/** F10: the module-scoped `require` function. */
interface EfxRequire {
  /** Resolve a relative (`./`, `../`) or root-relative specifier and return
   * its `module.exports` synchronously. */
  (specifier: string): unknown;
  /** Resolve a specifier to its root-relative module path. */
  resolve(specifier: string): string;
  /** Modules cached by resolved path. */
  readonly cache: Record<string, EfxModuleCacheEntry>;
}

/** F10: the module-scoped `module` object. */
interface EfxModule {
  /** The value `require` returns for this module. */
  exports: unknown;
  /** Root-relative resolved module path. */
  id: string;
  /** True once the module body has finished evaluating. */
  loaded: boolean;
}

declare const require: EfxRequire;
declare const module: EfxModule;
declare const exports: Record<string, unknown>;
declare const __filename: string;
declare const __dirname: string;
