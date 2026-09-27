// Type definitions for the EmotionFX script-facing API.
//
// This is a LIVING DOCUMENT: it describes the current public `efx` surface
// and grows with it. Update this file in the same change as any script-facing
// API change, alongside docs/js-api.md (see AGENTS.md).
//
// Status: F1, F2, F3, F4a, F4b, F5a, F5b, F6a are current behavior. F6b+
// entries are provisional and will be added when delivered.
//
// The declarations are global/ambient so they can be loaded verbatim into the
// gallery editor (Monaco `addExtraLib`) and type-checked by `tsc`.

type Color = [number, number, number, number];
type Vec2 = [number, number];
type Vec3 = [number, number, number];
type Mat4 = number[];
type Quat = number[];

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

interface CreateMeshDataOptions extends MeshSurfaceData {
  surfaces?: MeshSurfaceData[];
  /** One entry per surface; null selects the engine default material. */
  materials?: (Material | null)[];
}

interface DrawMeshOptions {
  mesh: EfxMesh;
  transform?: Mat4;
  color?: Color;
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
}
interface MakePlaneOptions {
  size?: number;
  segments?: number;
}
interface MakeSphereOptions {
  radius?: number;
  segments?: number;
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
  createTexture(imageData: EfxImageData): EfxTexture;
  readonly whiteTexture: EfxTexture;
  drawQuad(x: number, y: number, texture: EfxSample, opts?: DrawQuadOptions): void;
  setBlendMode(mode: 'alpha' | 'additive' | 'subtractive'): void;

  // F3 — 3D core
  setCamera3D(opts: Camera3DOptions): void;
  createMeshData(data: CreateMeshDataOptions): EfxMeshData;
  createMesh(meshData: EfxMeshData): EfxMesh;
  drawMesh(opts: DrawMeshOptions): void;
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
  loadTexture(path: string): EfxTexture;
}

declare const efx: Efx;
