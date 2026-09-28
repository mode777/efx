// Type definitions for the EmotionFX script-facing API.
//
// This is a LIVING DOCUMENT: it describes the current public `efx` surface
// and grows with it. Update this file in the same change as any script-facing
// API change, alongside docs/js-api.md (see AGENTS.md).
//
// Status: F1, F2, F3, F4a, F4b, F5a, F5b, F6a, F6b, F6c, F6e, F7, and F9 are
// current behavior. F6d (the `--repl [<root>]` interactive console) adds no
// API — it drives this same namespace from stdin; `.help`/`.exit` are host
// commands, not `efx` functions. F8 and later entries are provisional and will
// be added when delivered.
//
// The declarations are global/ambient so they can be loaded verbatim into the
// gallery editor (Monaco `addExtraLib`) and type-checked by `tsc`.

type Color = [number, number, number, number];
type Vec2 = [number, number];
type Vec3 = [number, number, number];
type Mat4 = number[];
type Quat = number[];

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

type EfxMouseButton = 'left' | 'right' | 'middle';
type EfxMod = 'shift' | 'ctrl' | 'alt' | 'super';

interface KeyboardDownEvent {
  key: EfxKey;
  repeat: boolean;
  mods: EfxMod[];
}
interface KeyboardUpEvent {
  key: EfxKey;
  mods: EfxMod[];
}
interface CharEvent {
  char: string;
}
interface MouseButtonEvent {
  button: EfxMouseButton;
  x: number;
  y: number;
  mods: EfxMod[];
}
interface MouseMoveEvent {
  x: number;
  y: number;
  dx: number;
  dy: number;
}
interface MouseWheelEvent {
  dx: number;
  dy: number;
}

/** F9 input namespaces (sub-members of the single `efx` object). */
interface EfxKeyboard {
  isDown(key: EfxKey): boolean;
  isPressed(key: EfxKey): boolean;
  isReleased(key: EfxKey): boolean;
  onDown(fn: (e: KeyboardDownEvent) => void): () => void;
  onUp(fn: (e: KeyboardUpEvent) => void): () => void;
  onChar(fn: (e: CharEvent) => void): () => void;
}

interface EfxMouse {
  isDown(button: EfxMouseButton): boolean;
  isPressed(button: EfxMouseButton): boolean;
  isReleased(button: EfxMouseButton): boolean;
  onDown(fn: (e: MouseButtonEvent) => void): () => void;
  onUp(fn: (e: MouseButtonEvent) => void): () => void;
  onMove(fn: (e: MouseMoveEvent) => void): () => void;
  onWheel(fn: (e: MouseWheelEvent) => void): () => void;
  readonly position: Vec2;
  readonly x: number;
  readonly y: number;
  readonly delta: Vec2;
  readonly wheel: Vec2;
}

interface EfxWindow {
  readonly size: Vec2;
  readonly width: number;
  readonly height: number;
  readonly dpiScale: number;
}

interface EfxImageData {
  readonly width: number;
  readonly height: number;
  destroy(): void;
}

interface EfxTexture {
  readonly width: number;
  readonly height: number;
  destroy(): void;
}

interface EfxRenderTarget {
  readonly width: number;
  readonly height: number;
  destroy(): void;
}

/** A live Texture or RenderTarget — accepted anywhere a texture is sampled. */
type EfxSample = EfxTexture | EfxRenderTarget;

interface EfxMeshData {
  readonly surfaceCount: number;
  destroy(): void;
}

interface EfxMesh {
  readonly surfaceCount: number;
  destroy(): void;
}

interface DrawQuadOptions {
  color?: Color;
  rotation?: number;
  scale?: number;
  size?: Vec2;
  origin?: Vec2;
  sourceRect?: { x: number; y: number; w: number; h: number };
}

interface Camera2DOptions {
  frame?: Vec2;
  x?: number;
  y?: number;
  zoom?: number;
  rotation?: number;
}

interface Camera3DOptions {
  pos: Vec3;
  target: Vec3;
  fov: number;
  near?: number;
  far?: number;
}

interface CreateImageDataOptions {
  width: number;
  height: number;
  pixels: number[] | Uint8Array;
  format?: 'rgba8';
}

type FlatNumbers = number[] | Float32Array;
type FlatIndices = number[] | Uint32Array;

interface MeshSurfaceData {
  positions: FlatNumbers;
  normals?: FlatNumbers;
  uvs?: FlatNumbers;
  colors?: FlatNumbers;
  /** Four joint indices per vertex (glTF JOINTS_0); requires `weights`. */
  joints?: FlatIndices;
  /** Four joint weights per vertex (glTF WEIGHTS_0); requires `joints`. */
  weights?: FlatNumbers;
  indices?: FlatIndices;
}

interface PhongChannel {
  color: Color;
  map?: EfxSample | null;
}

interface SpecularChannel {
  color: Color;
  shininess?: number;
  map?: EfxSample | null;
}

interface Material {
  ambient?: PhongChannel;
  diffuse?: PhongChannel;
  specular?: SpecularChannel;
  emissive?: PhongChannel;
  alphaMask?: EfxSample | null;
}

interface MeshDataBatch {
  surfaces: MeshSurfaceData[];
  /** One entry per surface; null selects the engine default material. */
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

interface MeshDataShorthand extends MeshSurfaceData {
  /** One entry; null selects the engine default material. */
  materials?: (Material | null)[];
  /** The batch field is forbidden in the shorthand form (exclusive union). */
  surfaces?: never;
}

type CreateMeshDataOptions = MeshDataBatch | MeshDataShorthand;

interface DrawMeshCallOptions {
  transform?: Mat4;
  color?: Color;
  /** F7: true draws the current CPU-posed vertices; absent/false the bind pose. */
  skinned?: boolean;
}

/** F7 pose sample: one clip sampled at `time` (seconds) with an optional
 * blend `weight` (normalized engine-side across an array; a single sample
 * ignores it). `clip` is a clip name or an index. */
interface PoseSample {
  clip: string | number;
  time: number;
  weight?: number;
}

interface PointLightOptions {
  pos: Vec3;
  color: Color;
  range?: number;
}

interface DirectionalLightOptions {
  dir: Vec3;
  color: Color;
}

interface MakeCubeOptions {
  size?: number;
  /** Material bound to the primitive's single surface; null = engine default. */
  material?: Material | null;
}
interface MakePlaneOptions {
  size?: number;
  segments?: number;
  /** Material bound to the primitive's single surface; null = engine default. */
  material?: Material | null;
}
interface MakeSphereOptions {
  radius?: number;
  segments?: number;
  /** Material bound to the primitive's single surface; null = engine default. */
  material?: Material | null;
}

type EfxPostEffect =
  | {
      effect: 'colorFilter';
      mix?: number;
      brightness?: number;
      contrast?: number;
      saturation?: number;
      tint?: Color;
    }
  | { effect: 'blur'; mix?: number; radius?: number }
  | { effect: 'bloom'; mix?: number; threshold?: number; strength?: number };

interface RenderScaleOptions {
  filter?: 'nearest' | 'linear';
}

interface TextureOptions {
  wrap?: 'repeat' | 'clamp' | 'mirror';
  filter?: 'linear' | 'nearest';
  mipmaps?: boolean;
}

interface LoadMeshDataOptions {
  mesh?: number | string;
}

interface EfxMat4 {
  identity(): Mat4;
  perspective(fovY: number, aspect: number, near: number, far: number): Mat4;
  ortho(w: number, h: number, near: number, far: number): Mat4;
  translate(m: Mat4, v: Vec3): Mat4;
  rotate(m: Mat4, deg: number, axis: Vec3): Mat4;
  scale(m: Mat4, v: Vec3): Mat4;
  multiply(a: Mat4, b: Mat4): Mat4;
}

interface EfxVec3 {
  add(a: Vec3, b: Vec3): Vec3;
  sub(a: Vec3, b: Vec3): Vec3;
  scale(v: Vec3, s: number): Vec3;
  normalize(v: Vec3): Vec3;
  cross(a: Vec3, b: Vec3): Vec3;
  dot(a: Vec3, b: Vec3): number;
}

interface EfxQuat {
  identity(): Quat;
  fromAxisAngle(deg: number, axis: Vec3): Quat;
  multiply(a: Quat, b: Quat): Quat;
  toMat4(q: Quat): Mat4;
}

interface Efx {
  // F1 — environment & lifecycle
  log(msg?: unknown): void;
  quit(code?: number): never;
  args(): string[];
  registerUpdateHook(fn: (dt: number) => void): () => void;
  registerRenderHook(fn: () => void): () => void;

  // F2 — 2D drawing
  setClearColor(color: Color): void;
  setCamera2D(opts: Camera2DOptions): void;
  createImageData(opts: CreateImageDataOptions): EfxImageData;
  createTexture(imageData: EfxImageData, opts?: TextureOptions): EfxTexture;
  readonly whiteTexture: EfxTexture;
  drawQuad(x: number, y: number, texture: EfxSample, opts?: DrawQuadOptions): void;
  setBlendMode(mode: 'alpha' | 'additive' | 'subtractive'): void;

  // F3 — 3D core
  setCamera3D(opts: Camera3DOptions): void;
  createMeshData(data: CreateMeshDataOptions): EfxMeshData;
  createMesh(meshData: EfxMeshData): EfxMesh;
  drawMesh(mesh: EfxMesh, opts?: DrawMeshCallOptions): void;
  makeCube(opts?: MakeCubeOptions): EfxMeshData;
  makePlane(opts?: MakePlaneOptions): EfxMeshData;
  makeSphere(opts?: MakeSphereOptions): EfxMeshData;

  // F3 — pure-JS math layer
  mat4: EfxMat4;
  vec3: EfxVec3;
  quat: EfxQuat;

  // F4a/F4b — lights & per-surface Phong materials
  setLight(slot: number, opts: PointLightOptions | null): void;
  setDirectionalLight(opts: DirectionalLightOptions | null): void;
  setMeshSurfaceMaterial(mesh: EfxMesh, surfaceIndex: number, mat: Material | null): void;

  // F5a — render targets
  createRenderTarget(opts: { width: number; height: number }): EfxRenderTarget;
  beginRenderTarget(rt: EfxRenderTarget): void;
  endRenderTarget(): void;

  // F5b — post effects & render scale
  setPostEffects(list: EfxPostEffect[] | null): void;
  setRenderScale(scale: number, opts?: RenderScaleOptions): void;

  // F6a — resource loading (paths relative to the resource root)
  loadText(path: string): string;
  loadImage(path: string): EfxImageData;
  // A texture is composed: createTexture(loadImage(path), opts) — no loader.

  // F6b — glTF 2.0 static import
  loadMeshData(path: string, opts?: LoadMeshDataOptions): EfxMeshData;

  // F7 — CPU skinning & animation (the script owns the clock)
  poseMesh(mesh: EfxMesh, pose: PoseSample | PoseSample[]): void;

  // F9 — input: sub-namespaces of the single efx object
  keyboard: EfxKeyboard;
  mouse: EfxMouse;
  window: EfxWindow;
}

declare const efx: Efx;
