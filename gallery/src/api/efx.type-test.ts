// Compile-time check that efx.d.ts describes the public API accurately.
// Checked by `tsc`/`svelte-check`; never imported at runtime.
export {};

const clear: Color = [0.05, 0.05, 0.1, 1];
efx.graphics.setClearColor(clear);

efx.graphics.setCamera3D([0, 1, 4], [0, 0, 0], 60);
efx.graphics.setLight(0, { pos: [1, 2, 3], color: [1, 1, 1, 1], range: 20 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1, -0.3], color: [0.2, 0.2, 0.3, 1] });
efx.graphics.setFog({ color: [0.55, 0.65, 0.75, 1], near: 8, far: 60 });
efx.graphics.setFog({ color: [0.1, 0.12, 0.15, 1], mode: 'exp2', density: 0.035 });
efx.graphics.setFog(null);

const cube: EfxMesh = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1 }));
cube.setSurfaceMaterial(0, {
  ambient: { color: [0.1, 0.1, 0.1, 1] },
  diffuse: { color: [1, 1, 1, 1] },
  specular: { color: [1, 1, 1, 1], shininess: 32 },
});
efx.graphics.drawMesh(cube, { transform: efx.math.mat4.rotate(efx.math.mat4.identity(), 45, [0, 1, 0]) });
efx.graphics.drawMesh(cube);

// Primitives bind an optional material to their single surface.
const mat: Material = { diffuse: { color: [0.8, 0.3, 0.2, 1] } };
const matBlend: Material = { blend: 'additive' };
const matInherit: Material = { blend: null };
const matUnlit: Material = { unlit: true, diffuse: { color: [1, 1, 1, 1] } };
const matAlphaModes: Material[] = [
  { alphaMode: 'opaque' },
  { alphaMode: 'mask', alphaMask: null },
  { alphaMode: 'blend', diffuse: { color: [1, 1, 1, 0.5] } },
  { alphaMode: null },
];
efx.graphics.makeCube({ size: 1, material: matAlphaModes[2] });
efx.graphics.makeSphere({ material: null });
// @ts-expect-error — unlit is a boolean flag
const matUnlitBad: Material = { unlit: 1 };
// @ts-expect-error — alphaMode is a mode string, not a boolean
const matAlphaBad: Material = { alphaMode: true };

// Inverted primitives (sky domes) and depthWrite:false (sky drawn first).
const skyCube: EfxMeshData = efx.graphics.makeCube({ size: 20, inverted: true });
const skySphere: EfxMeshData = efx.graphics.makeSphere({ radius: 50, inverted: true });
skyCube.destroy();
skySphere.destroy();
efx.graphics.drawMesh(cube, { depthWrite: false });
// @ts-expect-error — depthWrite is a boolean
efx.graphics.drawMesh(cube, { depthWrite: 1 });
// @ts-expect-error — inverted is a boolean
efx.graphics.makeCube({ inverted: 1 });

// Heightfields build MeshData from image luminance; the image is a required
// positional argument, options are the bag.
const heightImg: EfxImageData = efx.graphics.createImageData(
  4, 4, new Uint8Array(4 * 4 * 4));
const field: EfxMeshData = efx.graphics.makeHeightfield(heightImg);
const cropped: EfxMeshData = efx.graphics.makeHeightfield(heightImg, {
  region: [1, 1, 2, 2],
  step: 1,
  height: 5,
  size: 20,
  material: mat,
});
cropped.destroy();
// @ts-expect-error — the image is required
efx.graphics.makeHeightfield();
// @ts-expect-error — region is a 4-number tuple
efx.graphics.makeHeightfield(heightImg, { region: [0, 0, 2] });
// @ts-expect-error — unknown options throw
efx.graphics.makeHeightfield(heightImg, { frobnicate: 1 });

// The surface list is positional and the materials array is an optional second argument.
const batch: EfxMeshData = efx.graphics.createMeshData(
  [{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0], indices: [0, 1, 2] }],
  [mat],
);
efx.graphics.createMesh(batch).destroy();
batch.destroy();
// @ts-expect-error — the removed single-surface shorthand bag is not accepted
efx.graphics.createMeshData({ positions: [0, 0, 0] });
// @ts-expect-error — drawMesh takes the mesh positionally, not in the bag
efx.graphics.drawMesh({ mesh: cube });
// @ts-expect-error — the drawMesh option bag has no `mesh` field
efx.graphics.drawMesh(cube, { mesh: cube });

const rt: EfxRenderTarget = efx.graphics.createRenderTarget(64, 64);
efx.graphics.beginRenderTarget(rt);
efx.graphics.drawQuad(rt, 0, 0, { size: [64, 64] });
efx.graphics.endRenderTarget();
efx.graphics.setPostEffects([{ effect: 'bloom', threshold: 0.7, strength: 0.8, mix: 0.5 }]);
efx.graphics.setRenderScale(0.5, { filter: 'nearest' });

// mesh bounds + frustum culling — the bounds query, the cull option, the
// createMeshData bounds override
const cb: MeshBounds = cube.bounds;
const cbMin: Vec3 = cb.min;
const cbRadius: number = cb.radius;
efx.graphics.drawMesh(cube, { cull: false });
efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0] }], undefined, {
  bounds: { min: [0, 0, 0], max: [1, 1, 1] },
});
// @ts-expect-error — cull is a boolean
efx.graphics.drawMesh(cube, { cull: 1 });
// @ts-expect-error — bounds holds min and max boxes
efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0] }], null, {
  bounds: { min: [0, 0, 0] },
});

// F6b — glTF import and per-texture samplers
const imported: EfxMeshData = efx.graphics.loadMeshData('models/quad.glb');
efx.graphics.createMesh(imported).destroy();
imported.destroy();
efx.graphics.loadMeshData('models/quad.glb', { mesh: 'm' }).destroy();
efx.graphics.loadMeshData('models/quad.glb', { mesh: 0 }).destroy();
efx.graphics.createTexture(
  efx.graphics.createImageData(1, 1, [0, 0, 0, 0]),
  { wrap: 'clamp', filter: 'nearest', mipmaps: true },
).destroy();

// dynamic textures — streaming updates (dynamic-textures)
const dynImg: EfxImageData = efx.graphics.createImageData(2, 2, new Uint8Array(16));
const dyn: EfxTexture = efx.graphics.createTexture(dynImg, { dynamic: true });
dyn.update(dynImg);
dyn.destroy();
// @ts-expect-error — dynamic is a boolean
efx.graphics.createTexture(dynImg, { dynamic: 1 });
// @ts-expect-error — update takes an ImageData
dyn.update([0, 0, 0, 0]);

// The type document must reject a mistyped member and a malformed options bag.
// @ts-expect-error — unknown member
efx.notARealFunction();
// @ts-expect-error — setCamera3D requires positional pos/target/fov
efx.graphics.setCamera3D({});
// @ts-expect-error — createImageData requires positional width/height/pixels
efx.graphics.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0] });
// @ts-expect-error — blend mode is a fixed set
efx.graphics.setBlendMode('multiply');
// @ts-expect-error — unknown texture wrap value
efx.graphics.createTexture(efx.graphics.createImageData(1, 1, [0, 0, 0, 0]), { wrap: 'bogus' });
// @ts-expect-error — mipmaps is a boolean
efx.graphics.createTexture(efx.graphics.createImageData(1, 1, [0, 0, 0, 0]), { mipmaps: 'yes' });
// @ts-expect-error — loadMeshData mesh selector is a number or a name
efx.graphics.loadMeshData('models/quad.glb', { mesh: {} });

// F7 — posing takes a sample or an array; the skinned draw option is a boolean
cube.pose({ clip: 'Walk', time: 1 });
cube.pose([{ clip: 0, time: 1, weight: 0.5 }]);
efx.graphics.drawMesh(cube, { skinned: true });
efx.graphics.drawMesh(cube, { transform: efx.math.mat4.identity(), color: [1, 1, 1, 1], skinned: false });
// @ts-expect-error — an unknown pose sample field is rejected
cube.pose({ clip: 'Walk', time: 1, bogus: true });
// @ts-expect-error — skinned is a boolean
efx.graphics.drawMesh(cube, { skinned: 1 });

// F8a — font + text: data -> baked font -> draw/measure
const fontData: EfxFontData = efx.graphics.loadFontData('fonts/kenney.ttf');
const font: EfxFont = efx.graphics.createFont(fontData, 32, {
  glyphs: 'ABCabc 0123',
  padding: 1,
  filter: 'linear',
  outline: { width: 2 },
  shadow: { blur: 3, offset: [2, 2] },
});
const plainFont: EfxFont = efx.graphics.createFont(fontData, 16);
const bounds: TextBounds = efx.graphics.drawText('hello', font, 10, 10, {
  align: 'justify',
  valign: 'middle',
  width: 200,
  lineHeight: 40,
  color: [1, 1, 1, 1],
  outlineColor: [0, 0, 0, 1],
  shadowColor: [0, 0, 0, 1],
  rotation: 15,
  scale: 1.5,
});
const measured: TextBounds = font.measure('hello', { width: 200 });
bounds.lines;
measured.width;

// The operation free functions are gone — each is a method on its subject.
// @ts-expect-error — measureText moved to Font.measure
efx.graphics.measureText('hello', font, { width: 200 });
// @ts-expect-error — poseMesh moved to Mesh.pose
efx.graphics.poseMesh(cube, { clip: 'Walk', time: 1 });
// @ts-expect-error — setMeshSurfaceMaterial moved to Mesh.setSurfaceMaterial
efx.graphics.setMeshSurfaceMaterial(cube, 0, mat);
efx.graphics.drawText('plain', plainFont, 0, 0);
// @ts-expect-error — createFont requires a positional size
efx.graphics.createFont(fontData);
// @ts-expect-error — align is a fixed set
efx.graphics.drawText('x', font, 0, 0, { align: 'middle' });
// @ts-expect-error — drawText takes the font positionally
efx.graphics.drawText('x', 0, 0, { font });
// @ts-expect-error — unknown font option
efx.graphics.createFont(fontData, 12, { bogus: 1 });

// F11 — billboards, batched 2D sprites, and CPU particle systems
const f11tex: EfxTexture = efx.graphics.createTexture(
  efx.graphics.createImageData(1, 1, [0, 0, 0, 0]),
);
efx.graphics.drawBillboard(f11tex, [0, 1, 0], {
  size: [2, 3],
  facing: 'y',
  color: [1, 1, 1, 1],
  depthTest: true,
});
efx.graphics.drawBillboard(f11tex, [0, 1, 0], {
  facing: 'plane',
  normal: [0, 1, 0],
});
efx.graphics.drawSprites(f11tex, [{ x: 0, y: 0, size: [4, 4] }, { x: 8, y: 0, rotation: 45 }]);
const ps: EfxParticleSystem = efx.graphics.createParticleSystem(f11tex, 100, [1, 2], {
  emissionRate: 10,
  position: [0, 0, 0],
  direction: [0, 1, 0],
  speed: [1, 2],
  gravity: [0, -1, 0],
  sizes: [1, 3],
  colors: [[1, 0, 0, 1], [1, 1, 0, 0]],
  facing: 'plane',
  normal: [0, 1, 0],
  emissionShape: { shape: 'sphere', size: [1, 1, 1] },
  blend: 'additive',
});
ps.emit(10);
ps.set({ emissionRate: 0, position: [1, 0, 0] });
ps.speedScale = 2;
ps.count;
efx.graphics.drawParticles(ps);
ps.destroy();
// @ts-expect-error — createParticleSystem requires a positional lifetime
efx.graphics.createParticleSystem(f11tex, 10);
// @ts-expect-error — facing is a fixed set
efx.graphics.drawBillboard(f11tex, [0, 0, 0], { facing: 'sideways' });
// @ts-expect-error — every sprite needs x and y
efx.graphics.drawSprites(f11tex, [{ size: [4, 4] }]);

// Blend overrides are per-object; the frame state is `setBlendMode`.
efx.graphics.drawQuad(f11tex, 0, 0, { blend: 'additive' });
efx.graphics.drawSprites(f11tex, [{ x: 0, y: 0 }], { blend: 'subtractive' });
efx.graphics.drawBillboard(f11tex, [0, 0, 0], { blend: 'additive' });
efx.graphics.createParticleSystem(f11tex, 10, 1, { blend: null });
cube.setSurfaceMaterial(0, { blend: 'additive' });
// @ts-expect-error — blend is a fixed set
efx.graphics.drawQuad(f11tex, 0, 0, { blend: 'multiply' });
// @ts-expect-error — drawSprites options hold only blend
efx.graphics.drawSprites(f11tex, [{ x: 0, y: 0 }], { bogus: true });
// @ts-expect-error — particle blend is a mode or null
efx.graphics.createParticleSystem(f11tex, 10, 1, { blend: 'multiply' });

// F10 — CommonJS module authoring facilities: module-scoped, resolved from the
// resource root, and never members of `efx`.
const dep: unknown = require('./lib/math.js');
const resolved: string = require.resolve('./lib/math.js');
require.cache[resolved];
module.exports = dep;
exports.named = dep;
const here: string = __filename;
const dir: string = __dirname;
// @ts-expect-error — require takes a specifier string
require(42);
// @ts-expect-error — the module facilities are not part of the efx namespace
efx.require('./lib/math.js');

// Numeric tuples are fixed-length: colors are 3 or 4 components (RGB is
// opaque) and transforms are exactly 16 numbers, so arity mistakes are caught
// at compile time.
efx.graphics.setClearColor([0, 0, 0]);
efx.graphics.setClearColor([0, 0, 0, 1]);
// @ts-expect-error — a color holds 3 or 4 components
efx.graphics.setClearColor([0, 0]);
// @ts-expect-error — a transform is a fixed-length 16-number matrix
efx.graphics.drawMesh(cube, { transform: [1, 0, 0] });

// script-api-ergonomics surface: clock, one-call model matrix, scalar helpers,
// screen picking, plane tiling, collider teleport + debug view.
const nowSec: number = efx.time.now;
const model: Mat4 = efx.math.mat4.transform([1, 2, 3], 90, 0, 0, 2);
const model2: Mat4 = efx.math.mat4.transform([0, 0, 0], 45);
const clamped: number = efx.math.clamp(5, 0, 3);
const mixed: number = efx.math.lerp(0, 10, 0.5);
const ray: ScreenRay = efx.graphics.screenRay(...efx.mouse.position);
const tiledPlane: EfxMeshData = efx.graphics.makePlane({ uvs: [4, 4] });
// @ts-expect-error — transform requires a position
efx.math.mat4.transform();
nowSec; model; model2; clamped; mixed; ray; tiledPlane;

// ADR 0051 — math/io/color sub-namespaces and the args property. The moved
// names are gone from the root, so the old call shapes fail to compile.
const mv: Mat4 = efx.math.mat4.identity();
const vv: Vec3 = efx.math.vec3.add([1, 0, 0], [0, 1, 0]);
const qq: Quat = efx.math.quat.fromAxisAngle(90, [0, 1, 0]);
const text: string = efx.io.loadText('data.txt');
const bytes: Uint8Array = efx.io.loadData('blob.bin');
const white: Color = efx.color.white;
const transparent: Color = efx.color.transparent;
efx.graphics.setClearColor(efx.color.navy);
efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { color: efx.color.red });
const argv: string[] = efx.args;
mv; vv; qq; text; bytes; white; transparent; argv;
// @ts-expect-error — whiteTexture moved into efx.graphics
efx.whiteTexture;
// @ts-expect-error — loadText moved into efx.io
efx.loadText('data.txt');
// @ts-expect-error — mat4 moved into efx.math
efx.mat4.identity();
// @ts-expect-error — args is a property, not a function
efx.args();
// @ts-expect-error — efx.color holds constants, not a constructor
new efx.color();

// Physics — required inputs are positional, the trailing bag is all-optional.
const ground = efx.physics.createBody({ type: 'box', size: [40, 1, 40] }, { position: [0, -0.5, 0] });
const crate = efx.physics.createBody({ type: 'sphere', radius: 0.5 }, { dynamic: true, mass: 2 });
const hero = efx.physics.createCharacter(0.4, 1.8, { position: [-5, 1, 0] });
const hit = efx.physics.raycast([0, 1, 0], [1, 0, 0], 6, { mask: 1 });
const hits = efx.physics.raycast([0, 1, 0], [1, 0, 0], 6, { all: true });
efx.physics.debugDraw({ color: [1, 0, 0], alpha: 0.4 });
crate.setPosition([1, 2, 3]);
hero.setPosition([0, 0, 0]);
// @ts-expect-error — debugDraw alpha is a number
efx.physics.debugDraw({ alpha: 'half' });
// @ts-expect-error — setPosition requires a position vector
crate.setPosition();
ground; crate; hero; hit; hits;
// @ts-expect-error — the shape is positional, not a bag field
efx.physics.createBody({ shape: { type: 'sphere', radius: 1 } });
// @ts-expect-error — raycast requires a positional maxDistance
efx.physics.raycast([0, 1, 0], [1, 0, 0]);
// @ts-expect-error — createCharacter requires positional radius/height
efx.physics.createCharacter({ radius: 0.4, height: 1.8 });

// Noise — pure functions of seed + coordinates; raw per-type value ranges.
const hmap = new Float32Array(129 * 129);
efx.noise.fill2(hmap, 129, 129, { seed: 7, type: 'opensimplex2', frequency: 0.01, octaves: 5, lacunarity: 2, gain: 0.5, warp: 'none' });
const n2: number = efx.noise.noise2(0.5, 0.25);
const n2c: number = efx.noise.noise2(3, 4, { type: 'cellular', seed: 9 });
const n3: number = efx.noise.noise3(0.5, 0.25, 0.75, { warp: 'gradient' });
const nimg = efx.noise.imageData(64, 64, { seed: 9, type: 'cellular', domain: [-1, 0.15] });
const nimgAlpha = efx.noise.imageData(64, 64, { alpha: true });
const nimgAlphaOff = efx.noise.imageData(64, 64, { alpha: false });
n2; n2c; n3; hmap; nimg; nimgAlpha; nimgAlphaOff;
// @ts-expect-error — fill2 requires a Float32Array
efx.noise.fill2([0.1, 0.2], 2, 1);
// @ts-expect-error — the value buffer is positional, not a bag field
efx.noise.fill2({ buf: new Float32Array(4) }, 2, 2);
// @ts-expect-error — unknown noise type
efx.noise.noise2(0, 0, { type: 'worley' });
// @ts-expect-error — domain is a [lo, hi] tuple
efx.noise.imageData(4, 4, { domain: 3 });
// @ts-expect-error — alpha is a boolean flag
efx.noise.imageData(4, 4, { alpha: 'yes' });

// video-playback — MPEG-1 Video surface
const vid: EfxVideo = efx.video.openVideo('intro.mpg', { loop: true, muted: false });
vid.play();
vid.pause();
const vt: EfxTexture = vid.texture;
const vtime: number = vid.currentTime;
const vdur: number = vid.duration;
const vended: boolean = vid.ended;
vid.loop = false;
vid.muted = true;
vt; vtime; vdur; vended;
efx.graphics.drawQuad(vid.texture, 0, 0, { size: [vid.width, vid.height] });
vid.destroy();
// @ts-expect-error — openVideo requires a path string
efx.video.openVideo(5);
// @ts-expect-error — loop is a boolean
efx.video.openVideo('intro.mpg', { loop: 1 });
// @ts-expect-error — unknown video option
efx.video.openVideo('intro.mpg', { volume: 0.5 });
