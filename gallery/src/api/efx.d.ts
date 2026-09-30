// Type definitions for the EmotionFX script-facing API.
//
// This is a LIVING DOCUMENT: it describes the current public `efx` surface
// and grows with it. Update this file in the same change as any script-facing
// API change, then regenerate the committed reference docs/api/ from it
// (see AGENTS.md).
//
// It covers the whole current surface: environment and lifecycle hooks, 2D
// drawing, the 3D core and procedural primitives, lights and Phong materials,
// render targets and post effects, the resource/glTF import layer, CPU
// skinning, fonts and text, keyboard/mouse/gamepad input, CommonJS script
// modules, billboards/particles, and the physics world. The `--repl` console
// mode adds no API — it drives this same namespace from stdin.
//
// The declarations are global/ambient so they can be loaded verbatim into the
// gallery editor (Monaco `addExtraLib`) and type-checked by `tsc`.

/**
 * An RGBA color: four normalized floats in `0..1`, ordered `[r, g, b, a]`.
 * Most lighting and material channels ignore the alpha component.
 *
 * @example
 * ```js
 * efx.setClearColor([0.05, 0.05, 0.1, 1]);
 * efx.drawQuad(0, 0, tex, { color: [1, 0.5, 0, 1] });
 * ```
 */
type Color = [number, number, number, number];

/** A 2-component vector `[x, y]` (frame pixels for 2D APIs, world units for 3D). */
type Vec2 = [number, number];

/** A 3-component vector `[x, y, z]` in world units. */
type Vec3 = [number, number, number];

/**
 * A column-major 4×4 matrix as a flat 16-number array.
 *
 * @example
 * ```js
 * const model = efx.mat4.translate(
 *   efx.mat4.rotate(efx.mat4.identity(), 45, [0, 1, 0]),
 *   [0, 0.5, 0],
 * );
 * efx.drawMesh(mesh, { transform: model });
 * ```
 */
type Mat4 = [
  number, number, number, number,
  number, number, number, number,
  number, number, number, number,
  number, number, number, number,
];

/**
 * A quaternion `[x, y, z, w]`.
 *
 * @example
 * ```js
 * const q = efx.quat.fromAxisAngle(90, [0, 1, 0]);
 * const m = efx.quat.toMat4(q);
 * ```
 */
type Quat = [number, number, number, number];

// ---------------------------------------------------------------------------
// Input: keyboard, mouse & window
// ---------------------------------------------------------------------------

/** The engine-owned lowercase keyboard identifier set. */
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

/** The engine-owned mouse button identifier set. */
type EfxMouseButton = 'left' | 'right' | 'middle';

/** An active keyboard modifier name reported in an event's `mods`. */
type EfxMod = 'shift' | 'ctrl' | 'alt' | 'super';

/** Payload of a key-down event. */
interface KeyboardDownEvent {
  /** The key that went down. */
  key: EfxKey;
  /** `true` when this is an auto-repeat rather than the initial press. */
  repeat: boolean;
  /** Modifier keys held at the moment of the event. */
  mods: EfxMod[];
}

/** Payload of a key-up event. */
interface KeyboardUpEvent {
  /** The key that went up. */
  key: EfxKey;
  /** Modifier keys held at the moment of the event. */
  mods: EfxMod[];
}

/** Payload of a text-input event. */
interface CharEvent {
  /** The decoded character, e.g. `'A'` (may be more than one UTF-16 unit). */
  char: string;
}

/** Payload of a mouse button event. */
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

/** Payload of a mouse move event. */
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

/** Payload of a mouse wheel event. */
interface MouseWheelEvent {
  /** Horizontal scroll delta for this frame. */
  dx: number;
  /** Vertical scroll delta for this frame. */
  dy: number;
}

/**
 * Keyboard queries and event subscriptions. Queries report current frame
 * state; `isPressed`/`isReleased` are one-frame edges. Each event
 * registration returns an idempotent unsubscribe function.
 *
 * @example
 * ```js
 * efx.keyboard.onDown((e) => {
 *   if (e.repeat) return;
 *   if (e.key === 'space') nova(pointerX, pointerY);
 * });
 *
 * efx.registerUpdateHook(() => {
 *   if (efx.keyboard.isDown('left') || efx.keyboard.isDown('a')) wx -= 240;
 * });
 * ```
 */
interface EfxKeyboard {
  /**
   * Test whether a key is currently held.
   *
   * @param key - Key name to test.
   * @returns `true` while `key` is held; throws `TypeError` for an unknown key name.
   */
  isDown(key: EfxKey): boolean;
  /**
   * Test whether a key transitioned down this frame.
   *
   * @param key - Key name to test.
   * @returns `true` on the frame `key` transitioned down; throws `TypeError` for an unknown key.
   */
  isPressed(key: EfxKey): boolean;
  /**
   * Test whether a key transitioned up this frame.
   *
   * @param key - Key name to test.
   * @returns `true` on the frame `key` transitioned up; throws `TypeError` for an unknown key.
   */
  isReleased(key: EfxKey): boolean;
  /**
   * Subscribe to key-down events.
   *
   * @param fn - Called with each key-down event, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onDown(fn: (e: KeyboardDownEvent) => void): () => void;
  /**
   * Subscribe to key-up events.
   *
   * @param fn - Called with each key-up event, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onUp(fn: (e: KeyboardUpEvent) => void): () => void;
  /**
   * Subscribe to decoded text-input events.
   *
   * @param fn - Called with each character event, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onChar(fn: (e: CharEvent) => void): () => void;
}

/**
 * Mouse queries and event subscriptions. Queries report current frame state;
 * `isPressed`/`isReleased` are one-frame edges. Each event registration
 * returns an idempotent unsubscribe function. All coordinates are surface
 * (framebuffer) pixels with a top-left origin and y down — the same space as
 * `drawQuad` and the 2D frame.
 *
 * @example
 * ```js
 * efx.mouse.onMove((e) => { pointerX = e.x; pointerY = e.y; });
 * efx.mouse.onWheel((e) => { brush *= (1 - e.dy * 0.09); });
 *
 * efx.registerUpdateHook(() => {
 *   if (efx.mouse.isDown('left')) well = 1;
 * });
 * ```
 */
interface EfxMouse {
  /**
   * Test whether a mouse button is currently held.
   *
   * @param button - Button name to test.
   * @returns `true` while `button` is held; throws `TypeError` for an unknown button.
   */
  isDown(button: EfxMouseButton): boolean;
  /**
   * Test whether a mouse button transitioned down this frame.
   *
   * @param button - Button name to test.
   * @returns `true` on the frame `button` transitioned down.
   */
  isPressed(button: EfxMouseButton): boolean;
  /**
   * Test whether a mouse button transitioned up this frame.
   *
   * @param button - Button name to test.
   * @returns `true` on the frame `button` transitioned up.
   */
  isReleased(button: EfxMouseButton): boolean;
  /**
   * Subscribe to button-down events.
   *
   * @param fn - Called with each button-down event, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onDown(fn: (e: MouseButtonEvent) => void): () => void;
  /**
   * Subscribe to button-up events.
   *
   * @param fn - Called with each button-up event, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onUp(fn: (e: MouseButtonEvent) => void): () => void;
  /**
   * Subscribe to move events.
   *
   * @param fn - Called with each move event, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onMove(fn: (e: MouseMoveEvent) => void): () => void;
  /**
   * Subscribe to wheel events.
   *
   * @param fn - Called with each wheel event, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
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

/**
 * Read-only window metrics, in surface (framebuffer) pixels. On high-DPI
 * displays the surface is larger than the logical window; `dpiScale` is the
 * surface-to-logical ratio.
 *
 * @example
 * ```js
 * const [w, h] = efx.window.size;
 * const logicalW = w / efx.window.dpiScale;
 * ```
 */
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
// Gamepad input
// ---------------------------------------------------------------------------

/** The engine-owned semantic gamepad button identifier set. */
type EfxGamepadButton =
  | 'south' | 'east' | 'west' | 'north'
  | 'leftShoulder' | 'rightShoulder' | 'leftTrigger' | 'rightTrigger'
  | 'back' | 'start' | 'guide' | 'leftStick' | 'rightStick'
  | 'dpadUp' | 'dpadDown' | 'dpadLeft' | 'dpadRight';

/** The engine-owned semantic gamepad axis identifier set. */
type EfxGamepadAxis =
  | 'leftX' | 'leftY' | 'rightX' | 'rightY' | 'leftTrigger' | 'rightTrigger';

/**
 * A pad slot view: plain data plus query methods. Not a resource — there is
 * nothing to create or destroy.
 */
interface EfxGamepadView {
  /** Slot index this view reports (0-based). */
  readonly index: number;
  /** `true` while a pad occupies this slot. */
  readonly connected: boolean;
  /** Device name string reported by the platform. */
  readonly name: string;
  /** `true` when a semantic mapping was found (else only `rawButton`/`rawAxis`). */
  readonly mapped: boolean;
  /**
   * Test whether a semantic button is currently held.
   *
   * @param button - Semantic button name.
   * @returns `true` while `button` is held; throws `TypeError` for an unknown button.
   */
  isDown(button: EfxGamepadButton): boolean;
  /**
   * Test whether a semantic button transitioned down this frame.
   *
   * @param button - Semantic button name.
   * @returns `true` on the frame `button` transitioned down.
   */
  isPressed(button: EfxGamepadButton): boolean;
  /**
   * Test whether a semantic button transitioned up this frame.
   *
   * @param button - Semantic button name.
   * @returns `true` on the frame `button` transitioned up.
   */
  isReleased(button: EfxGamepadButton): boolean;
  /**
   * Read a normalized axis value.
   *
   * @param axis - Semantic axis name.
   * @returns Stick axes in `-1..1` and trigger axes in `0..1`; throws `TypeError` for an unknown axis.
   */
  axis(axis: EfxGamepadAxis): number;
  /**
   * Read a raw device button value by index (for unmapped pads).
   *
   * @param index - Raw button index.
   * @returns The raw device value (0 when out of range).
   */
  rawButton(index: number): number;
  /**
   * Read a raw device axis value by index (for unmapped pads).
   *
   * @param index - Raw axis index.
   * @returns The raw device value (0 when out of range).
   */
  rawAxis(index: number): number;
}

/**
 * Gamepad queries and connect/disconnect events over a fixed engine-owned
 * bank of four pad slots, reported by index.
 *
 * @example
 * ```js
 * efx.gamepad.onConnect((pad) => efx.log('pad: ' + pad.name));
 * efx.registerUpdateHook(() => {
 *   const pad = efx.gamepad.get(0);
 *   if (!pad) return;
 *   if (pad.isDown('rightTrigger')) boost = 1;
 *   x += pad.axis('leftX') * speed * dt;
 * });
 * ```
 */
interface EfxGamepad {
  /** Number of currently connected pads. */
  readonly count: number;
  /**
   * Get the pad view for a slot.
   *
   * @param index - Slot index (0-based).
   * @returns The pad view, or `null` when the slot is empty; throws `TypeError` for a non-numeric index.
   */
  get(index: number): EfxGamepadView | null;
  /**
   * Subscribe to pad-connect events.
   *
   * @param fn - Called with the pad view when a pad connects, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onConnect(fn: (pad: EfxGamepadView) => void): () => void;
  /**
   * Subscribe to pad-disconnect events.
   *
   * @param fn - Called with the pad view when a pad disconnects, before the update hooks.
   * @returns An idempotent unsubscribe function.
   */
  onDisconnect(fn: (pad: EfxGamepadView) => void): () => void;
}

// ---------------------------------------------------------------------------
// Resource types
// ---------------------------------------------------------------------------

/** Raw CPU pixels plus size and format (opaque native-backed class). */
interface EfxImageData {
  /** Image width in pixels. Throws `TypeError` when destroyed. */
  readonly width: number;
  /** Image height in pixels. Throws `TypeError` when destroyed. */
  readonly height: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** A GPU texture (opaque native-backed class). */
interface EfxTexture {
  /** Texture width in pixels. Throws `TypeError` when destroyed. */
  readonly width: number;
  /** Texture height in pixels. Throws `TypeError` when destroyed. */
  readonly height: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** A GPU render target with color and depth attachments (opaque native-backed class). */
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

/** CPU mesh data holding 1..16 surfaces (opaque native-backed class). */
interface EfxMeshData {
  /** Number of surfaces (1..16). */
  readonly surfaceCount: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** A GPU mesh uploaded from MeshData (opaque native-backed class). */
interface EfxMesh {
  /** Number of surfaces (1..16). */
  readonly surfaceCount: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

// ---------------------------------------------------------------------------
// Font + text
// ---------------------------------------------------------------------------

/** A parsed TrueType/OpenType font, CPU only (opaque native-backed class). */
interface EfxFontData {
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** A baked glyph atlas plus layout metrics (opaque native-backed class). */
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

/** A baked outline ring around glyphs. */
interface FontOutline {
  /** Outline thickness in pixels (must be > 0). */
  width: number;
}

/** A baked blurred shadow behind glyphs. */
interface FontShadow {
  /** Blur radius in pixels (must be > 0). */
  blur: number;
  /** Pixel offset `[dx, dy]`; defaults to `[0, 0]`. */
  offset?: Vec2;
}

/**
 * Options for `createFont`.
 *
 * @example
 * ```js
 * const title = efx.createFont(efx.loadFontData('font.ttf'), {
 *   size: 44,
 *   outline: { width: 2 },
 *   shadow: { blur: 3, offset: [2, 2] },
 * });
 * const body = efx.createFont(efx.loadFontData('font.ttf'), { size: 24 });
 * ```
 */
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

/**
 * Options for `drawText` / `measureText`.
 *
 * @example
 * ```js
 * efx.drawText(paragraph, body, 40, 168, {
 *   width: 560,
 *   align: 'justify',
 *   color: [0.85, 0.88, 0.95, 1],
 * });
 * ```
 */
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

/** Laid-out text bounds returned by `drawText` / `measureText`. */
interface TextBounds {
  /** Laid-out width in pixels. */
  readonly width: number;
  /** Laid-out height in pixels. */
  readonly height: number;
  /** Number of laid-out lines. */
  readonly lines: number;
}

// ---------------------------------------------------------------------------
// 2D drawing
// ---------------------------------------------------------------------------

/**
 * Options for `drawQuad`.
 *
 * @example
 * ```js
 * // a 48x48 tinted sprite (see the "Bouncing Sprites" sample)
 * efx.drawQuad(d.x, d.y, tex, { size: [48, 48], color: d.c });
 * // a cropped atlas region with an explicit pivot
 * efx.drawQuad(160, 16, tex, {
 *   size: [128, 128],
 *   sourceRect: { x: 128, y: 128, w: 256, h: 256 },
 * });
 * ```
 */
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

/**
 * Options for `setCamera2D`.
 *
 * @example
 * ```js
 * efx.setCamera2D({ frame: [640, 480] });          // virtual 640x480 frame
 * efx.setCamera2D({ frame: [640, 480], zoom: 2 }); // 2x zoom about the center
 * ```
 */
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

/**
 * Options for `createImageData`.
 *
 * @example
 * ```js
 * // a procedural radial glow (see the "Particle Showcase" sample)
 * const size = 32;
 * const px = new Uint8Array(size * size * 4);
 * for (let y = 0; y < size; y++) {
 *   for (let x = 0; x < size; x++) {
 *     const i = (y * size + x) * 4;
 *     px[i] = px[i + 1] = px[i + 2] = 255;
 *     px[i + 3] = 255; // ...compute coverage from the distance to center
 *   }
 * }
 * const glow = efx.createImageData({ width: size, height: size, pixels: px });
 * ```
 */
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

/**
 * Sampler options for `createTexture`.
 *
 * @example
 * ```js
 * // tiled, minified ground texture: repeat wrap plus a mip chain
 * const tex = efx.createTexture(efx.loadImage('paving_color.jpg'),
 *                               { mipmaps: true });
 * ```
 */
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
// 3D core
// ---------------------------------------------------------------------------

/**
 * Options for `setCamera3D`.
 *
 * @example
 * ```js
 * efx.setCamera3D({ pos: [0, 1.6, 4.2], target: [0, 0, 0], fov: 60 });
 * ```
 */
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

/** One mesh surface's attribute arrays (a Godot surface / glTF primitive). */
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

/** An ambient/diffuse/emissive Phong channel. */
interface PhongChannel {
  /** Channel color `[r, g, b, a]` (alpha ignored by shading). */
  color: Color;
  /** Optional modulating texture (or render target); `null`/omitted means none. */
  map?: EfxSample | null;
}

/** The specular Phong channel (adds a shininess exponent). */
interface SpecularChannel {
  /** Specular color `[r, g, b, a]` (alpha ignored by shading). */
  color: Color;
  /** Specular exponent (default 32). */
  shininess?: number;
  /** Optional modulating texture (or render target); `null`/omitted means none. */
  map?: EfxSample | null;
}

/**
 * A per-surface Phong material. It is JS-managed (no native handle, no
 * `destroy()`) and the engine snapshots it at binding time, so later mutation
 * of the script object does not change the bound material.
 *
 * @example
 * ```js
 * efx.setMeshSurfaceMaterial(cube, 0, {
 *   ambient:  { color: [0.12, 0.12, 0.16, 1] },
 *   diffuse:  { color: [1, 1, 1, 1] },
 *   specular: { color: [1, 1, 1, 1], shininess: 32 },
 *   emissive: { color: [0, 0, 0, 1] },
 * });
 * ```
 */
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

/** The multi-surface batch form of `createMeshData`. */
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

/** The single-surface shorthand form of `createMeshData`. */
interface MeshDataShorthand extends MeshSurfaceData {
  /** One entry; `null` selects the engine default material. */
  materials?: (Material | null)[];
  /** The batch field is forbidden in the shorthand form (exclusive union). */
  surfaces?: never;
}

/**
 * `createMeshData` accepts either the batch or the shorthand form.
 *
 * @example
 * ```js
 * // shorthand: one surface
 * const quad = efx.createMeshData({
 *   positions: [-4, 0, -4, 4, 0, -4, 4, 0, 4, -4, 0, 4],
 *   normals:   [0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0],
 *   uvs:       [0, 0, 6, 0, 6, 6, 0, 6],
 *   indices:   [0, 1, 2, 0, 2, 3],
 * });
 *
 * // batch: several surfaces with per-surface materials
 * const mesh = efx.createMeshData({
 *   surfaces: [{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0] }],
 *   materials: [null],
 * });
 * ```
 */
type CreateMeshDataOptions = MeshDataBatch | MeshDataShorthand;

/**
 * Options for `drawMesh`.
 *
 * @example
 * ```js
 * efx.drawMesh(cube, {
 *   transform: efx.mat4.rotate(efx.mat4.identity(), yaw, [0, 1, 0]),
 *   color: [0.95, 0.5, 0.2, 1],
 * });
 * ```
 */
interface DrawMeshCallOptions {
  /** Column-major transform (default identity). */
  transform?: Mat4;
  /** Tint multiplying vertex colors (default opaque white). */
  color?: Color;
  /** `true` draws the current CPU-posed vertices; absent/false the bind pose. */
  skinned?: boolean;
}

/**
 * Options for `makeCube`.
 *
 * @example
 * ```js
 * const cube = efx.createMesh(efx.makeCube({ size: 1.4 }));
 * ```
 */
interface MakeCubeOptions {
  /** Edge length (default 1, must be > 0). */
  size?: number;
  /** Material bound to the primitive's single surface; `null` = engine default. */
  material?: Material | null;
}

/**
 * Options for `makePlane`.
 *
 * @example
 * ```js
 * const ground = efx.createMesh(efx.makePlane({ size: 10, segments: 4 }));
 * ```
 */
interface MakePlaneOptions {
  /** Edge length (default 1, must be > 0). */
  size?: number;
  /** Grid subdivisions per side (positive integer, default 1). */
  segments?: number;
  /** Material bound to the primitive's single surface; `null` = engine default. */
  material?: Material | null;
}

/**
 * Options for `makeSphere`.
 *
 * @example
 * ```js
 * const ball = efx.createMesh(efx.makeSphere({ radius: 1.6, segments: 32 }));
 * ```
 */
interface MakeSphereOptions {
  /** Sphere radius (default 1, must be > 0). */
  radius?: number;
  /** Longitude/latitude subdivisions (positive integer, default 16). */
  segments?: number;
  /** Material bound to the primitive's single surface; `null` = engine default. */
  material?: Material | null;
}

/**
 * Options for `makeCapsule`.
 *
 * @example
 * ```js
 * // a capsule matching a physics character (radius 0.4, height 1.8)
 * const body = efx.createMesh(efx.makeCapsule({ radius: 0.4, height: 1.8 }));
 * ```
 */
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
// Lights
// ---------------------------------------------------------------------------

/**
 * Options for `setLight` (a point light).
 *
 * @example
 * ```js
 * efx.setLight(0, { pos: [3, 4, 2], color: [1, 0.95, 0.9, 1], range: 20 });
 * ```
 */
interface PointLightOptions {
  /** Light position in world units. */
  pos: Vec3;
  /** Light color `[r, g, b, a]` (alpha ignored). */
  color: Color;
  /** Attenuation radius (finite, >= 0; default 0 = no falloff). */
  range?: number;
}

/**
 * Options for `setDirectionalLight`.
 *
 * @example
 * ```js
 * efx.setDirectionalLight({ dir: [-0.5, -1, -0.3], color: [0.2, 0.25, 0.35, 1] });
 * ```
 */
interface DirectionalLightOptions {
  /** Direction the light travels (the direction to the light is `-dir`). */
  dir: Vec3;
  /** Light color `[r, g, b, a]` (alpha ignored). */
  color: Color;
}

// ---------------------------------------------------------------------------
// Render targets
// ---------------------------------------------------------------------------

/**
 * Options for `createRenderTarget`.
 *
 * @example
 * ```js
 * const scene = efx.createRenderTarget({ width: 512, height: 512 });
 * ```
 */
interface RenderTargetOptions {
  /** Target width in pixels (positive integer, 1..4096). */
  width: number;
  /** Target height in pixels (positive integer, 1..4096). */
  height: number;
}

// ---------------------------------------------------------------------------
// Post effects & render scale
// ---------------------------------------------------------------------------

/** A color-filter post effect (identity with all defaults). */
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

/** A separable gaussian blur post effect. */
interface BlurPostEffect {
  /** Discriminator selecting the blur effect. */
  effect: 'blur';
  /** Input/output blend in `0..1` (default 1). */
  mix?: number;
  /** Blur radius in scene pixels (finite, > 0 and <= 64; default 1). */
  radius?: number;
}

/** A bloom post effect. */
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

/** One entry of the declarative post-effect chain (`setPostEffects`). */
type EfxPostEffect = ColorFilterPostEffect | BlurPostEffect | BloomPostEffect;

/**
 * Options for `setRenderScale`.
 *
 * @example
 * ```js
 * efx.setRenderScale(0.5, { filter: 'nearest' }); // crisp half-res pixels
 * efx.setRenderScale(1);                          // back to native
 * ```
 */
interface RenderScaleOptions {
  /** Final blit filter (default `'linear'`). */
  filter?: 'nearest' | 'linear';
}

// ---------------------------------------------------------------------------
// Resource loading
// ---------------------------------------------------------------------------

/**
 * Options for `loadMeshData`.
 *
 * @example
 * ```js
 * const first = efx.loadMeshData('scene.gltf');
 * const named = efx.loadMeshData('scene.gltf', { mesh: 'Teapot' });
 * const byIndex = efx.loadMeshData('scene.gltf', { mesh: 2 });
 * ```
 */
interface LoadMeshDataOptions {
  /** Mesh selector: a non-negative index or a mesh name; defaults to the first mesh. */
  mesh?: number | string;
}

// ---------------------------------------------------------------------------
// Skinning & animation
// ---------------------------------------------------------------------------

/**
 * One pose sample: a clip sampled at `time` (seconds) with an optional blend
 * `weight` (normalized engine-side across an array; a single sample ignores
 * it). `clip` is a clip name or an index.
 *
 * @example
 * ```js
 * // cross-fade walk -> run over two seconds
 * efx.poseMesh(hero, [
 *   { clip: 'Walk', time: t, weight: 1 - k },
 *   { clip: 'Run',  time: t, weight: k },
 * ]);
 * ```
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
// Billboards, 2D sprites & CPU particles
// ---------------------------------------------------------------------------

/** Quad render mode for world-space billboards and particles. */
type EfxFacing = 'view' | 'y' | 'plane';

/** Blend mode for 2D quads, sprites, and particle batches. */
type EfxBlendMode = 'alpha' | 'additive' | 'subtractive';

/**
 * Options for `drawBillboard`.
 *
 * @example
 * ```js
 * efx.drawBillboard([0, 0.4, 0], {
 *   texture: spark,
 *   size: 0.9,
 *   color: [1, 0.7, 0.3, 0.9],
 * });
 * ```
 */
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

/**
 * One entry of a `drawSprites` batch; each is equivalent to a `drawQuad` call.
 *
 * @example
 * ```js
 * efx.drawSprites(spark, [
 *   { x: 20,  y: 20, size: [48, 48], color: [1, 0.4, 0.2, 0.9] },
 *   { x: 74,  y: 20, size: [48, 48], color: [1, 0.7, 0.3, 0.9] },
 * ]);
 * ```
 */
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

/** Emission volume for a particle system. */
interface EmissionShapeOptions {
  /** Shape kind. */
  shape: 'point' | 'box' | 'sphere' | 'sphereSurface' | 'disc';
  /** Shape extent `[x, y, z]` (default `[0, 0, 0]`). */
  size?: Vec3;
}

/**
 * Options for `createParticleSystem` (`texture`, `max`, and `lifetime` are
 * required).
 *
 * @example
 * ```js
 * // an additive fire (see the "Particle Showcase" sample)
 * const fire = efx.createParticleSystem({
 *   texture: spark,
 *   max: 600,
 *   lifetime: [0.4, 0.9],
 *   emissionRate: 140,
 *   position: [0, 0.1, 0],
 *   direction: [0, 1, 0],
 *   spread: 22,
 *   speed: [0.8, 1.8],
 *   gravity: [0, 0.6, 0],
 *   sizes: [0.55, 0.05],
 *   colors: [[1, 0.9, 0.45, 0.95], [1, 0.25, 0.05, 0]],
 *   blend: 'additive',
 *   facing: 'view',
 * });
 * ```
 */
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

/** Partial update bag for `ParticleSystem.set`. */
type ParticleSystemSetOptions = Partial<ParticleSystemOptions>;

/** A native-backed CPU particle system. */
interface EfxParticleSystem {
  /**
   * Emit a burst of particles immediately.
   *
   * @param n - Number of particles to emit.
   */
  emit(n: number): void;
  /** Start continuous emission. */
  start(): void;
  /** Stop emitting (live particles keep simulating). */
  stop(): void;
  /** Pause simulation. */
  pause(): void;
  /** Reset the system to its initial state. */
  reset(): void;
  /**
   * Apply a partial options update atomically.
   *
   * @param opts - Any subset of the creation options to change.
   */
  set(opts: ParticleSystemSetOptions): void;
  /** Number of live particles. */
  readonly count: number;
  /** Read-write simulated-time factor. */
  speedScale: number;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

// ---------------------------------------------------------------------------
// Pure-JS math layer
// ---------------------------------------------------------------------------

/** Pure-JS 4×4 matrix helpers (inputs are never mutated). */
interface EfxMat4 {
  /**
   * Build the identity matrix.
   *
   * @returns A new identity `Mat4`.
   */
  identity(): Mat4;
  /**
   * Build a perspective projection (column-major, GL convention).
   *
   * @param fovY - Vertical field of view in degrees.
   * @param aspect - Viewport aspect ratio (width / height).
   * @param near - Near plane distance.
   * @param far - Far plane distance.
   * @returns A new projection `Mat4`.
   */
  perspective(fovY: number, aspect: number, near: number, far: number): Mat4;
  /**
   * Build an orthographic projection.
   *
   * @param w - View width in world units.
   * @param h - View height in world units.
   * @param near - Near plane distance.
   * @param far - Far plane distance.
   * @returns A new orthographic `Mat4`.
   */
  ortho(w: number, h: number, near: number, far: number): Mat4;
  /**
   * Translate a matrix.
   *
   * @param m - Source matrix.
   * @param v - Translation `[x, y, z]`.
   * @returns A new translated `Mat4`.
   */
  translate(m: Mat4, v: Vec3): Mat4;
  /**
   * Rotate a matrix about an axis.
   *
   * @param m - Source matrix.
   * @param deg - Rotation angle in degrees.
   * @param axis - Rotation axis.
   * @returns A new rotated `Mat4`.
   */
  rotate(m: Mat4, deg: number, axis: Vec3): Mat4;
  /**
   * Scale a matrix.
   *
   * @param m - Source matrix.
   * @param v - Scale factors `[x, y, z]`.
   * @returns A new scaled `Mat4`.
   */
  scale(m: Mat4, v: Vec3): Mat4;
  /**
   * Multiply two matrices.
   *
   * @param a - Left-hand matrix.
   * @param b - Right-hand matrix.
   * @returns The product `a · b` (b applies to a vector first).
   */
  multiply(a: Mat4, b: Mat4): Mat4;
}

/** Pure-JS 3-component vector helpers (inputs are never mutated). */
interface EfxVec3 {
  /**
   * Add two vectors.
   *
   * @param a - Left operand.
   * @param b - Right operand.
   * @returns `a + b`.
   */
  add(a: Vec3, b: Vec3): Vec3;
  /**
   * Subtract two vectors.
   *
   * @param a - Left operand.
   * @param b - Right operand.
   * @returns `a - b`.
   */
  sub(a: Vec3, b: Vec3): Vec3;
  /**
   * Scale a vector.
   *
   * @param v - Vector to scale.
   * @param s - Scalar factor.
   * @returns `v * s`.
   */
  scale(v: Vec3, s: number): Vec3;
  /**
   * Normalize a vector.
   *
   * @param v - Vector to normalize.
   * @returns The unit vector along `v`.
   */
  normalize(v: Vec3): Vec3;
  /**
   * Compute the cross product.
   *
   * @param a - Left operand.
   * @param b - Right operand.
   * @returns `a × b`.
   */
  cross(a: Vec3, b: Vec3): Vec3;
  /**
   * Compute the dot product.
   *
   * @param a - Left operand.
   * @param b - Right operand.
   * @returns `a · b`.
   */
  dot(a: Vec3, b: Vec3): number;
}

/** Pure-JS quaternion helpers (inputs are never mutated). */
interface EfxQuat {
  /**
   * Build the identity quaternion.
   *
   * @returns A new identity `Quat`.
   */
  identity(): Quat;
  /**
   * Build a quaternion from an axis and an angle.
   *
   * @param deg - Rotation angle in degrees.
   * @param axis - Rotation axis.
   * @returns The corresponding `Quat`.
   */
  fromAxisAngle(deg: number, axis: Vec3): Quat;
  /**
   * Multiply two quaternions.
   *
   * @param a - Left operand.
   * @param b - Right operand.
   * @returns The product `a · b`.
   */
  multiply(a: Quat, b: Quat): Quat;
  /**
   * Convert a quaternion to a rotation matrix.
   *
   * @param q - Quaternion to convert.
   * @returns The equivalent rotation `Mat4`.
   */
  toMat4(q: Quat): Mat4;
}

// ---------------------------------------------------------------------------
// Collision, character & impulse dynamics
// ---------------------------------------------------------------------------

/** A sphere collider. */
interface SphereShape {
  /** Discriminator selecting the sphere shape. */
  type: 'sphere';
  /** Sphere radius (must be > 0). */
  radius: number;
}

/** An axis-aligned box collider. */
interface BoxShape {
  /** Discriminator selecting the box shape. */
  type: 'box';
  /** Full box extent `[x, y, z]` (each component > 0). */
  size: Vec3;
}

/** A vertical capsule collider. */
interface CapsuleShape {
  /** Discriminator selecting the capsule shape. */
  type: 'capsule';
  /** Capsule radius (must be > 0). */
  radius: number;
  /** Total tip-to-tip height including caps; must be >= 2 * radius. */
  height: number;
}

/** A static triangle-mesh collider built from a live Mesh. */
interface MeshShape {
  /** Discriminator selecting the triangle-mesh shape. */
  type: 'mesh';
  /** A live Mesh (static bodies and queries only). */
  mesh: EfxMesh;
}

/**
 * A collision shape accepted by bodies and by the spatial queries.
 *
 * @example
 * ```js
 * const box  = { type: 'box', size: [1, 1, 1] };
 * const ball = { type: 'sphere', radius: 0.5 };
 * const hero = { type: 'capsule', radius: 0.4, height: 1.8 };
 * const ramp = { type: 'mesh', mesh: rampMesh };
 * ```
 */
type PhysicsShape = SphereShape | BoxShape | CapsuleShape | MeshShape;

/**
 * Options for `physics.createBody`.
 *
 * @example
 * ```js
 * const crate = efx.physics.createBody({
 *   dynamic: true, mass: 2, friction: 0.6, restitution: 0.1,
 *   shape: { type: 'box', size: [1, 1, 1] }, position: [0, 3, 0],
 * });
 * ```
 */
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

/**
 * Options for `physics.createStaticMesh`.
 *
 * @example
 * ```js
 * const ramp = efx.physics.createStaticMesh(rampMesh, { friction: 0.8 });
 * ```
 */
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

/**
 * Options for `physics.createCharacter`.
 *
 * @example
 * ```js
 * const hero = efx.physics.createCharacter({
 *   radius: 0.4, height: 1.8, position: [-5, 1, 0],
 *   floorMaxAngle: 50, stepHeight: 0.35, floorSnapLength: 0.15,
 * });
 * ```
 */
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

/** One contact reported on a dynamic body's `contacts` list. */
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

/** A native-backed collider in the single physics world. */
interface EfxBody {
  /** Read-only world position (mutate `velocity` to move a dynamic body). */
  readonly position: Vec3;
  /** Read-write linear velocity. */
  velocity: Vec3;
  /** Read-only column-major translation matrix, usable directly by `drawMesh`. */
  readonly transform: Mat4;
  /** Read-only contacts from the last `step`; valid until the next `step`. */
  readonly contacts: PhysicsContact[];
  /**
   * Apply an instantaneous impulse.
   *
   * @param v - Impulse vector in world units.
   */
  applyImpulse(v: Vec3): void;
  /**
   * Apply a force for the next `step`.
   *
   * @param v - Force vector in world units.
   */
  applyForce(v: Vec3): void;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** One collision reported by `Character.moveAndSlide`. */
interface PhysicsMoveCollision {
  /** The blocking collider's handle (`null` for a static mesh or character). */
  readonly body: EfxBody | null;
  /** Collision normal `[x, y, z]`. */
  readonly normal: Vec3;
  /** Collision point `[x, y, z]`. */
  readonly point: Vec3;
}

/** The result of `Character.moveAndSlide`. */
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

/** A native-backed kinematic capsule character controller. */
interface EfxCharacter {
  /** Read-only world position. */
  readonly position: Vec3;
  /** Read-write velocity (drives one-way pushes during `step`). */
  velocity: Vec3;
  /** Read-only: true when currently standing on a floor. */
  readonly onFloor: boolean;
  /**
   * Sweep and slide the capsule.
   *
   * @param motion - Desired displacement for this call, in world units.
   * @returns The move result: position, floor/wall/ceiling flags, and collisions.
   */
  moveAndSlide(motion: Vec3): PhysicsMoveResult;
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/**
 * Options for `physics.raycast`.
 *
 * @example
 * ```js
 * const hit = efx.physics.raycast(hero.position, [1, 0, 0], { maxDistance: 6 });
 * if (hit) efx.log('hit at ' + hit.distance.toFixed(2));
 * ```
 */
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

/** One raycast hit. */
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

/** Options for `physics.overlap`. */
interface OverlapOptions {
  /** Query position in world units (default `[0, 0, 0]`). */
  position?: Vec3;
  /** Collision mask bitmask filter. */
  mask?: number;
}

/** Options for `physics.shapeCast`. */
interface ShapeCastOptions {
  /** Collision mask bitmask filter. */
  mask?: number;
  /** Include sensors (excluded by default). */
  sensors?: boolean;
}

/** One shape-cast hit. */
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

/**
 * The single physics world. The script owns stepping: call `step(dt)` each
 * frame and the engine never advances the world on its own.
 *
 * @example
 * ```js
 * efx.physics.gravity = [0, -9.81, 0];
 * const ground = efx.physics.createBody({
 *   shape: { type: 'box', size: [40, 1, 40] }, position: [0, -0.5, 0] });
 * const crate = efx.physics.createBody({
 *   dynamic: true, mass: 2,
 *   shape: { type: 'box', size: [1, 1, 1] }, position: [0, 3, 0] });
 * const hero = efx.physics.createCharacter({ radius: 0.4, height: 1.8 });
 *
 * efx.registerUpdateHook((dt) => {
 *   efx.physics.step(dt);
 *   hero.moveAndSlide([1.5 * dt, -9.81 * dt, 0]);
 * });
 * ```
 */
interface EfxPhysics {
  /** World gravity `[x, y, z]` (read-write; default `[0, -9.81, 0]`). */
  gravity: Vec3;
  /** Solver iteration count (read-write positive integer; default 8). */
  iterations: number;
  /**
   * Advance the world.
   *
   * @param dt - Time step in seconds.
   */
  step(dt: number): void;
  /** Remove every collider from the world. */
  clear(): void;
  /**
   * Create a static, dynamic, or sensor body.
   *
   * @param opts - Body options; `shape` is required.
   * @returns The new body handle.
   */
  createBody(opts: CreateBodyOptions): EfxBody;
  /**
   * Create a static triangle-mesh collider from a live Mesh.
   *
   * @param mesh - Source mesh (arbitrary surface count).
   * @param opts - Optional placement and material options.
   * @returns The new static body handle.
   */
  createStaticMesh(mesh: EfxMesh, opts?: CreateStaticMeshOptions): EfxBody;
  /**
   * Create a kinematic capsule character controller.
   *
   * @param opts - Character options; `radius` and `height` are required.
   * @returns The new character handle.
   */
  createCharacter(opts: CreateCharacterOptions): EfxCharacter;
  /**
   * Cast a ray and return the nearest hit.
   *
   * @param origin - Ray origin in world units.
   * @param direction - Ray direction (normalized by the engine).
   * @param opts - Query options; `maxDistance` is required.
   * @returns The first hit, or `null` when nothing is hit.
   */
  raycast(origin: Vec3, direction: Vec3, opts: RaycastOptions): PhysicsRayHit | null;
  /**
   * Cast a ray and return every hit sorted by distance.
   *
   * @param origin - Ray origin in world units.
   * @param direction - Ray direction (normalized by the engine).
   * @param opts - Query options with `all: true`; `maxDistance` is required.
   * @returns Every hit sorted by distance.
   */
  raycast(origin: Vec3, direction: Vec3, opts: RaycastOptions & { all: true }): PhysicsRayHit[];
  /**
   * Find bodies and characters intersecting a shape (including sensors).
   *
   * @param shape - Query shape.
   * @param opts - Optional query position and mask.
   * @returns The live handles that intersect `shape`.
   */
  overlap(shape: PhysicsShape, opts?: OverlapOptions): (EfxBody | EfxCharacter)[];
  /**
   * Sweep a shape and return the first hit.
   *
   * @param shape - Shape to sweep.
   * @param from - Sweep start in world units.
   * @param motion - Sweep displacement in world units.
   * @param opts - Optional mask and sensor inclusion.
   * @returns The first hit, or `null` when nothing is hit.
   */
  shapeCast(shape: PhysicsShape, from: Vec3, motion: Vec3,
            opts?: ShapeCastOptions): PhysicsShapeHit | null;
}

// ---------------------------------------------------------------------------
// The single `efx` namespace
// ---------------------------------------------------------------------------

/**
 * The engine-provided script surface; the only global scripts use.
 *
 * @example
 * ```js
 * // the smallest complete 3D scene (the "Hello Cube" sample)
 * efx.setClearColor([0.03, 0.04, 0.09, 1]);
 * efx.setCamera3D({ pos: [0, 1.6, 4.2], target: [0, 0, 0], fov: 60 });
 * efx.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
 * efx.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });
 *
 * const cube = efx.createMesh(efx.makeCube({ size: 1.4 }));
 * efx.setMeshSurfaceMaterial(cube, 0, {
 *   ambient:  { color: [0.12, 0.12, 0.16, 1] },
 *   diffuse:  { color: [1, 1, 1, 1] },
 *   specular: { color: [1, 1, 1, 1], shininess: 32 },
 *   emissive: { color: [0, 0, 0, 1] },
 * });
 *
 * let t = 0;
 * function update(dt) { t += dt; }
 * function render() {
 *   const model = efx.mat4.rotate(efx.mat4.identity(), t * 40, [0, 1, 0]);
 *   efx.drawMesh(cube, { transform: model, color: [0.95, 0.5, 0.2, 1] });
 * }
 * ```
 */
interface Efx {
  // Environment & lifecycle

  /**
   * Print a message to stdout followed by a newline and flush.
   *
   * @param msg - Value to print; non-strings use their standard string representation, and omitting it prints an empty line.
   */
  log(msg?: unknown): void;
  /**
   * Request engine termination with an exit code.
   *
   * @param code - Exit code (default 0).
   * @returns Never returns normally: the engine unwinds and exits with `code`.
   */
  quit(code?: number): never;
  /**
   * Get the host arguments passed to the script run.
   *
   * @returns The `--script <file> [args...]` tail, or an empty array when none were given.
   */
  args(): string[];
  /**
   * Register a per-frame update hook.
   *
   * @param fn - Called once per frame with `dt` seconds since the previous frame (0 on the first).
   * @returns An idempotent unsubscribe function.
   */
  registerUpdateHook(fn: (dt: number) => void): () => void;
  /**
   * Register a per-frame render hook.
   *
   * @param fn - Called once per frame after update hooks; takes no arguments.
   * @returns An idempotent unsubscribe function.
   */
  registerRenderHook(fn: () => void): () => void;

  // 2D drawing

  /**
   * Set the frame clear color.
   *
   * @param color - Clear color `[r, g, b, a]` (default black).
   */
  setClearColor(color: Color): void;
  /**
   * Set the 2D virtual pixel frame and its projection state.
   *
   * @param opts - Frame, center, zoom, and rotation.
   */
  setCamera2D(opts: Camera2DOptions): void;
  /**
   * Build CPU pixels as an ImageData.
   *
   * @param opts - Width, height, RGBA8 pixels, and optional format.
   * @returns The new ImageData.
   */
  createImageData(opts: CreateImageDataOptions): EfxImageData;
  /**
   * Upload ImageData to a GPU texture.
   *
   * @param imageData - Source pixels.
   * @param opts - Optional wrap, filter, and mipmap settings.
   * @returns The new texture.
   */
  createTexture(imageData: EfxImageData, opts?: TextureOptions): EfxTexture;
  /** Engine-owned 1×1 white texture (read-only; `destroy()` throws). */
  readonly whiteTexture: EfxTexture;
  /**
   * Record one textured quad.
   *
   * @param x - Quad top-left x in frame pixels.
   * @param y - Quad top-left y in frame pixels.
   * @param texture - Live texture or render target to sample.
   * @param opts - Optional tint, transform, size, origin, and source rect.
   */
  drawQuad(x: number, y: number, texture: EfxSample, opts?: DrawQuadOptions): void;
  /**
   * Set the blend mode for subsequently recorded 2D draws.
   *
   * @param mode - `'alpha'` (default), `'additive'`, or `'subtractive'`.
   */
  setBlendMode(mode: EfxBlendMode): void;

  // 3D core

  /**
   * Set the single 3D camera (separate from the 2D frame).
   *
   * @param opts - Camera position, target, field of view, and clip planes.
   */
  setCamera3D(opts: Camera3DOptions): void;
  /**
   * Build multi-surface MeshData from the batch or shorthand form.
   *
   * @param data - Surface attributes and optional per-surface materials.
   * @returns The new CPU MeshData.
   */
  createMeshData(data: CreateMeshDataOptions): EfxMeshData;
  /**
   * Upload all surfaces of MeshData to a GPU mesh.
   *
   * @param meshData - Source CPU mesh data.
   * @returns The new GPU mesh.
   */
  createMesh(meshData: EfxMeshData): EfxMesh;
  /**
   * Draw a whole mesh, depth-tested, under the recorded 3D camera.
   *
   * @param mesh - Live mesh to draw (required positional argument).
   * @param opts - Optional transform, tint, and skinned flag.
   */
  drawMesh(mesh: EfxMesh, opts?: DrawMeshCallOptions): void;
  /**
   * Build single-surface cube MeshData.
   *
   * @param opts - Optional size and bound material.
   * @returns The new CPU MeshData.
   */
  makeCube(opts?: MakeCubeOptions): EfxMeshData;
  /**
   * Build single-surface plane MeshData (on XZ, facing +Y).
   *
   * @param opts - Optional size, segments, and bound material.
   * @returns The new CPU MeshData.
   */
  makePlane(opts?: MakePlaneOptions): EfxMeshData;
  /**
   * Build single-surface UV sphere MeshData.
   *
   * @param opts - Optional radius, segments, and bound material.
   * @returns The new CPU MeshData.
   */
  makeSphere(opts?: MakeSphereOptions): EfxMeshData;
  /**
   * Build single-surface vertical capsule MeshData.
   *
   * @param opts - Optional radius, height, segments, and bound material.
   * @returns The new CPU MeshData.
   */
  makeCapsule(opts?: MakeCapsuleOptions): EfxMeshData;

  // Pure-JS math layer

  /** Pure-JS 4×4 matrix helpers. */
  mat4: EfxMat4;
  /** Pure-JS 3-component vector helpers. */
  vec3: EfxVec3;
  /** Pure-JS quaternion helpers. */
  quat: EfxQuat;

  // Lights & per-surface Phong materials

  /**
   * Set a point-light slot.
   *
   * @param slot - Slot index `0..3`.
   * @param opts - Light options, or `null` to disable the slot.
   */
  setLight(slot: number, opts: PointLightOptions | null): void;
  /**
   * Set the single directional light.
   *
   * @param opts - Light options, or `null` to disable it.
   */
  setDirectionalLight(opts: DirectionalLightOptions | null): void;
  /**
   * Bind a Phong material to one mesh surface.
   *
   * @param mesh - Owning live mesh.
   * @param surfaceIndex - Surface to bind (`0`-based).
   * @param mat - Material object, or `null` to restore the engine default.
   */
  setMeshSurfaceMaterial(mesh: EfxMesh, surfaceIndex: number, mat: Material | null): void;

  // Render targets

  /**
   * Create a GPU render target with a color and depth attachment.
   *
   * @param opts - Target width and height (1..4096 each).
   * @returns The new render target.
   */
  createRenderTarget(opts: RenderTargetOptions): EfxRenderTarget;
  /**
   * Redirect subsequently recorded draws into a render target, clearing it on entry.
   *
   * @param rt - Target to draw into.
   */
  beginRenderTarget(rt: EfxRenderTarget): void;
  /** Return to the default target. */
  endRenderTarget(): void;

  // Post effects & render scale

  /**
   * Set the declarative post-effect chain.
   *
   * @param list - Up to 8 effect entries, or `null`/`[]` to clear the chain.
   */
  setPostEffects(list: EfxPostEffect[] | null): void;
  /**
   * Set the scene-resolution scale and final blit filter.
   *
   * @param scale - Scene resolution ratio in `(0, 2]` (default 1).
   * @param opts - Optional blit filter.
   */
  setRenderScale(scale: number, opts?: RenderScaleOptions): void;

  // Resource loading (paths relative to the resource root)

  /**
   * Read a UTF-8 text resource.
   *
   * @param path - Resource-root-relative path.
   * @returns The decoded text.
   */
  loadText(path: string): string;
  /**
   * Decode a PNG/JPEG image resource to RGBA8.
   *
   * @param path - Resource-root-relative path.
   * @returns The decoded ImageData.
   */
  loadImage(path: string): EfxImageData;

  // glTF 2.0 static import

  /**
   * Import a glTF/GLB mesh (materials converted and bound per surface).
   *
   * @param path - Resource-root-relative path.
   * @param opts - Optional mesh selector.
   * @returns The imported CPU MeshData.
   */
  loadMeshData(path: string, opts?: LoadMeshDataOptions): EfxMeshData;

  // Font + text (C-implemented mid-level facilities)

  /**
   * Parse a `.ttf`/`.otf` font into FontData.
   *
   * @param path - Resource-root-relative path.
   * @returns The parsed FontData.
   */
  loadFontData(path: string): EfxFontData;
  /**
   * Bake a fixed glyph atlas from FontData.
   *
   * @param fontData - Parsed source font.
   * @param opts - Required bake options (at minimum `size`).
   * @returns The baked Font.
   */
  createFont(fontData: EfxFontData, opts: CreateFontOptions): EfxFont;
  /**
   * Lay out and record 2D text quads.
   *
   * @param text - Text to draw (supports newlines).
   * @param font - Baked font to draw with.
   * @param x - Anchor x in frame pixels.
   * @param y - Anchor y in frame pixels.
   * @param opts - Optional alignment, wrap, colors, rotation, and scale.
   * @returns The laid-out bounds.
   */
  drawText(text: string, font: EfxFont, x: number, y: number,
           opts?: TextOptions): TextBounds;
  /**
   * Lay out text without drawing it.
   *
   * @param text - Text to measure.
   * @param font - Baked font to measure with.
   * @param opts - Optional alignment, wrap, and scale (matching a later draw).
   * @returns The laid-out bounds.
   */
  measureText(text: string, font: EfxFont, opts?: TextOptions): TextBounds;

  // CPU skinning & animation (the script owns the clock)

  /**
   * CPU-pose a skinned mesh in place.
   *
   * @param mesh - Live skinned mesh.
   * @param pose - One pose sample, or an array of samples to blend.
   */
  poseMesh(mesh: EfxMesh, pose: PoseSample | PoseSample[]): void;

  // Input: sub-namespaces of the single efx object

  /** Keyboard queries and events. */
  keyboard: EfxKeyboard;
  /** Mouse queries and events. */
  mouse: EfxMouse;
  /** Read-only window metrics. */
  window: EfxWindow;
  /** Gamepad bank queries and connect/disconnect events. */
  gamepad: EfxGamepad;

  // World-space billboards, batched 2D sprites, CPU particles

  /**
   * Record one world-space billboard quad.
   *
   * @param pos - World position `[x, y, z]`.
   * @param opts - Required texture plus size, tint, facing, and depth options.
   */
  drawBillboard(pos: Vec3, opts: DrawBillboardOptions): void;
  /**
   * Record a batch of 2D sprite quads from one texture.
   *
   * @param texture - Live texture or render target to sample.
   * @param sprites - One options bag per quad; validation is atomic.
   */
  drawSprites(texture: EfxSample, sprites: SpriteOptions[]): void;
  /**
   * Create a native-backed CPU particle system.
   *
   * @param opts - System options; `texture`, `max`, and `lifetime` are required.
   * @returns The new particle system.
   */
  createParticleSystem(opts: ParticleSystemOptions): EfxParticleSystem;
  /**
   * Record one batch for a particle system's live particles.
   *
   * @param system - System whose live particles to draw.
   */
  drawParticles(system: EfxParticleSystem): void;

  // Collision, character & impulse dynamics (single world, script-stepped)

  /** The single physics world. */
  physics: EfxPhysics;

  // Audio playback (engine-owned mixing; no channels or voices in scripts)

  /** Streamed background music and sound effects. */
  audio: EfxAudio;
}

// ---------------------------------------------------------------------------
// Audio playback (F14)
// ---------------------------------------------------------------------------

/** Options for `audio.playSound` and `audio.playAudioEffect`. */
interface PlaySoundOptions {
  /** Linear gain (default `1`); negative values are clamped to `0`. */
  volume?: number;
  /** Stereo pan in `[-1, 1]` (default `0` = center). */
  pan?: number;
  /** Playback-rate multiplier (default `1`); values `<= 0` are treated as `1`. */
  pitch?: number;
  /** Loop until stopped (default `false`). */
  loop?: boolean;
}

/** Options for `audio.playBackgroundMusic`. */
interface PlayMusicOptions {
  /** Linear gain (default `1`); negative values are clamped to `0`. */
  volume?: number;
  /** Loop the track (default `false`). */
  loop?: boolean;
}

/** Decoded PCM sound data loaded from the resource root (opaque native-backed class). */
interface EfxSoundData {
  /** Release the native storage deterministically and idempotently. */
  destroy(): void;
}

/** One playing sound-effect voice (opaque native-backed class). */
interface EfxSound {
  /** Whether this voice is still playing (false once it ends or is stolen). */
  readonly playing: boolean;
  /** Linear gain. Setting a negative value throws `RangeError`. */
  volume: number;
  /** Stereo pan in `[-1, 1]`; out-of-range values are clamped by the mixer. */
  pan: number;
  /** Playback-rate multiplier; setting a non-positive value throws `RangeError`. */
  pitch: number;
  /** Stop this voice immediately. */
  stop(): void;
  /** Stop and release the handle deterministically and idempotently. */
  destroy(): void;
}

/** The streamed background-music source (opaque native-backed class). */
interface EfxMusic {
  /** Whether the background music is currently playing. */
  readonly playing: boolean;
  /** Stop the background music. */
  stop(): void;
  /** Pause the background music. */
  pause(): void;
  /** Resume paused background music. */
  resume(): void;
  /** Set the linear gain; a negative value throws `RangeError`. */
  setVolume(volume: number): void;
  /** Stop and release the handle deterministically and idempotently. */
  destroy(): void;
}

/**
 * Audio playback. The engine owns all mixing: scripts never see channels,
 * buses, or buffers. WAV and MP3 resources are supported; only decoded PCM is
 * played (no sequenced/modular formats). One background-music stream is active
 * at a time; a fixed bank of 32 sound-effect voices is mixed, with a
 * deterministic steal policy when all are busy.
 */
interface EfxAudio {
  /**
   * Decode a WAV or MP3 resource into sound data.
   *
   * @param path - Root-relative resource path.
   * @returns The decoded sound data; throws `Error` when it cannot be read or decoded, `TypeError` for a non-string path.
   */
  loadSoundData(path: string): EfxSoundData;
  /**
   * Start a decoded sound as a sound-effect voice.
   *
   * @param sound - Sound data from `loadSoundData`.
   * @param opts - Volume, pan, pitch, and loop options.
   * @returns The playing handle, or `null` when no voice is available (all busy and looping, or no audio device).
   */
  playSound(sound: EfxSoundData, opts?: PlaySoundOptions): EfxSound | null;
  /**
   * Load (with path caching) and start a sound effect.
   *
   * @param path - Root-relative resource path.
   * @param opts - Volume, pan, pitch, and loop options.
   * @returns The playing handle, or `null` when no voice is available.
   */
  playAudioEffect(path: string, opts?: PlaySoundOptions): EfxSound | null;
  /**
   * Start streamed background music, replacing any current track.
   *
   * @param path - Root-relative resource path.
   * @param opts - Volume and loop options.
   * @returns The music handle; throws `Error` when it cannot be read or decoded.
   */
  playBackgroundMusic(path: string, opts?: PlayMusicOptions): EfxMusic;
  /** Stop the active background music. */
  stopBackgroundMusic(): void;
  /** Unlock/resume audio after a user gesture (web autoplay); a no-op on desktop. */
  resume(): void;
}

declare const efx: Efx;

// ---------------------------------------------------------------------------
// CommonJS module authoring facilities
// ---------------------------------------------------------------------------
//
// Every script file under the resource root is a module; `require`/`module`/
// `exports` exist only inside a module's own scope (never on `efx` and never as
// true globals), and `require` loads synchronously from the resource root.
// TypeScript authors normally write `import`/`export` and let `tsc`
// (`module: commonjs`) emit the `require` form.

/** One entry of `require.cache`. */
interface EfxModuleCacheEntry {
  /** Root-relative resolved module path. */
  id: string;
  /** The module's `exports` value. */
  exports: unknown;
  /** `true` once the module body has finished evaluating. */
  loaded: boolean;
}

/**
 * The module-scoped `require` function. It resolves relative (`./`, `../`) or
 * root-relative specifiers and loads synchronously.
 *
 * @example
 * ```js
 * const palette = require('./lib/palette.js'); // relative module
 * const orbit = require('./lib/orbit');        // no extension -> .js fallback
 * const scene = require('./data/scene.json');  // JSON module -> parsed value
 * ```
 */
interface EfxRequire {
  /**
   * Load a module and return its exports.
   *
   * @param specifier - Relative (`./`, `../`) or root-relative module path.
   * @returns The module's `module.exports` value.
   */
  (specifier: string): unknown;
  /**
   * Resolve a specifier to its canonical module path.
   *
   * @param specifier - Specifier to resolve.
   * @returns The root-relative resolved module path.
   */
  resolve(specifier: string): string;
  /** Modules cached by resolved path. */
  readonly cache: Record<string, EfxModuleCacheEntry>;
}

/** The module-scoped `module` object. */
interface EfxModule {
  /** The value `require` returns for this module. */
  exports: unknown;
  /** Root-relative resolved module path. */
  id: string;
  /** `true` once the module body has finished evaluating. */
  loaded: boolean;
}

declare const require: EfxRequire;
declare const module: EfxModule;
declare const exports: Record<string, unknown>;
declare const __filename: string;
declare const __dirname: string;
