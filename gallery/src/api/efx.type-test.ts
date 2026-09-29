// Compile-time check that efx.d.ts describes the public API accurately.
// Checked by `tsc`/`svelte-check`; never imported at runtime.
export {};

const clear: Color = [0.05, 0.05, 0.1, 1];
efx.setClearColor(clear);

efx.setCamera3D({ pos: [0, 1, 4], target: [0, 0, 0], fov: 60 });
efx.setLight(0, { pos: [1, 2, 3], color: [1, 1, 1, 1], range: 20 });
efx.setDirectionalLight({ dir: [-0.4, -1, -0.3], color: [0.2, 0.2, 0.3, 1] });

const cube: EfxMesh = efx.createMesh(efx.makeCube({ size: 1 }));
efx.setMeshSurfaceMaterial(cube, 0, {
  ambient: { color: [0.1, 0.1, 0.1, 1] },
  diffuse: { color: [1, 1, 1, 1] },
  specular: { color: [1, 1, 1, 1], shininess: 32 },
});
efx.drawMesh(cube, { transform: efx.mat4.rotate(efx.mat4.identity(), 45, [0, 1, 0]) });
efx.drawMesh(cube);

// Primitives bind an optional material to their single surface.
const mat: Material = { diffuse: { color: [0.8, 0.3, 0.2, 1] } };
efx.makeCube({ size: 1, material: mat });
efx.makeSphere({ material: null });

// The batch form does not require shorthand fields; mixing the forms is rejected.
const batch: EfxMeshData = efx.createMeshData({
  surfaces: [{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0], indices: [0, 1, 2] }],
  materials: [mat],
});
efx.createMesh(batch).destroy();
batch.destroy();
// @ts-expect-error — surfaces and single-surface fields cannot be combined
efx.createMeshData({ surfaces: [{ positions: [0, 0, 0] }], positions: [0, 0, 0] });
// @ts-expect-error — drawMesh takes the mesh positionally, not in the bag
efx.drawMesh({ mesh: cube });
// @ts-expect-error — the drawMesh option bag has no `mesh` field
efx.drawMesh(cube, { mesh: cube });

const rt: EfxRenderTarget = efx.createRenderTarget({ width: 64, height: 64 });
efx.beginRenderTarget(rt);
efx.drawQuad(0, 0, rt, { size: [64, 64] });
efx.endRenderTarget();
efx.setPostEffects([{ effect: 'bloom', threshold: 0.7, strength: 0.8, mix: 0.5 }]);
efx.setRenderScale(0.5, { filter: 'nearest' });

// F6b — glTF import and per-texture samplers
const imported: EfxMeshData = efx.loadMeshData('models/quad.glb');
efx.createMesh(imported).destroy();
imported.destroy();
efx.loadMeshData('models/quad.glb', { mesh: 'm' }).destroy();
efx.loadMeshData('models/quad.glb', { mesh: 0 }).destroy();
efx.createTexture(
  efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0] }),
  { wrap: 'clamp', filter: 'nearest', mipmaps: true },
).destroy();

// The type document must reject a mistyped member and a malformed options bag.
// @ts-expect-error — unknown member
efx.notARealFunction();
// @ts-expect-error — setCamera3D requires pos/target/fov
efx.setCamera3D({});
// @ts-expect-error — blend mode is a fixed set
efx.setBlendMode('multiply');
// @ts-expect-error — unknown texture wrap value
efx.createTexture(efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0] }), { wrap: 'bogus' });
// @ts-expect-error — mipmaps is a boolean
efx.createTexture(efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0] }), { mipmaps: 'yes' });
// @ts-expect-error — loadMeshData mesh selector is a number or a name
efx.loadMeshData('models/quad.glb', { mesh: {} });

// F7 — posing takes a sample or an array; the skinned draw option is a boolean
efx.poseMesh(cube, { clip: 'Walk', time: 1 });
efx.poseMesh(cube, [{ clip: 0, time: 1, weight: 0.5 }]);
efx.drawMesh(cube, { skinned: true });
efx.drawMesh(cube, { transform: efx.mat4.identity(), color: [1, 1, 1, 1], skinned: false });
// @ts-expect-error — an unknown pose sample field is rejected
efx.poseMesh(cube, { clip: 'Walk', time: 1, bogus: true });
// @ts-expect-error — skinned is a boolean
efx.drawMesh(cube, { skinned: 1 });

// F8a — font + text: data -> baked font -> draw/measure
const fontData: EfxFontData = efx.loadFontData('fonts/kenney.ttf');
const font: EfxFont = efx.createFont(fontData, {
  size: 32,
  glyphs: 'ABCabc 0123',
  padding: 1,
  filter: 'linear',
  outline: { width: 2 },
  shadow: { blur: 3, offset: [2, 2] },
});
const plainFont: EfxFont = efx.createFont(fontData, { size: 16 });
const bounds: TextBounds = efx.drawText('hello', font, 10, 10, {
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
const measured: TextBounds = efx.measureText('hello', font, { width: 200 });
bounds.lines;
measured.width;
efx.drawText('plain', plainFont, 0, 0);
// @ts-expect-error — createFont requires a size
efx.createFont(fontData, {});
// @ts-expect-error — align is a fixed set
efx.drawText('x', font, 0, 0, { align: 'middle' });
// @ts-expect-error — drawText takes the font positionally
efx.drawText('x', 0, 0, { font });
// @ts-expect-error — unknown font option
efx.createFont(fontData, { size: 12, bogus: 1 });

// F11 — billboards, batched 2D sprites, and CPU particle systems
const f11tex: EfxTexture = efx.createTexture(
  efx.createImageData({ width: 1, height: 1, pixels: [0, 0, 0, 0] }),
);
efx.drawBillboard([0, 1, 0], {
  texture: f11tex,
  size: [2, 3],
  facing: 'y',
  color: [1, 1, 1, 1],
  depthTest: true,
});
efx.drawSprites(f11tex, [{ x: 0, y: 0, size: [4, 4] }, { x: 8, y: 0, rotation: 45 }]);
const ps: EfxParticleSystem = efx.createParticleSystem({
  texture: f11tex,
  max: 100,
  lifetime: [1, 2],
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
efx.drawParticles(ps);
ps.destroy();
// @ts-expect-error — createParticleSystem requires a lifetime
efx.createParticleSystem({ texture: f11tex, max: 10 });
// @ts-expect-error — facing is a fixed set
efx.drawBillboard([0, 0, 0], { texture: f11tex, facing: 'sideways' });
// @ts-expect-error — every sprite needs x and y
efx.drawSprites(f11tex, [{ size: [4, 4] }]);

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
