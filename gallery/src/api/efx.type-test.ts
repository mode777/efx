// Compile-time check that efx.d.ts describes the public API accurately.
// Checked by `tsc`/`svelte-check`; never imported at runtime.
export {};

const clear: Color = [0.05, 0.05, 0.1, 1];
efx.graphics.setClearColor(clear);

efx.graphics.setCamera3D([0, 1, 4], [0, 0, 0], 60);
efx.graphics.setLight(0, { pos: [1, 2, 3], color: [1, 1, 1, 1], range: 20 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1, -0.3], color: [0.2, 0.2, 0.3, 1] });

const cube: EfxMesh = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1 }));
efx.graphics.setMeshSurfaceMaterial(cube, 0, {
  ambient: { color: [0.1, 0.1, 0.1, 1] },
  diffuse: { color: [1, 1, 1, 1] },
  specular: { color: [1, 1, 1, 1], shininess: 32 },
});
efx.graphics.drawMesh(cube, { transform: efx.math.mat4.rotate(efx.math.mat4.identity(), 45, [0, 1, 0]) });
efx.graphics.drawMesh(cube);

// Primitives bind an optional material to their single surface.
const mat: Material = { diffuse: { color: [0.8, 0.3, 0.2, 1] } };
efx.graphics.makeCube({ size: 1, material: mat });
efx.graphics.makeSphere({ material: null });

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
efx.graphics.poseMesh(cube, { clip: 'Walk', time: 1 });
efx.graphics.poseMesh(cube, [{ clip: 0, time: 1, weight: 0.5 }]);
efx.graphics.drawMesh(cube, { skinned: true });
efx.graphics.drawMesh(cube, { transform: efx.math.mat4.identity(), color: [1, 1, 1, 1], skinned: false });
// @ts-expect-error — an unknown pose sample field is rejected
efx.graphics.poseMesh(cube, { clip: 'Walk', time: 1, bogus: true });
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
const measured: TextBounds = efx.graphics.measureText('hello', font, { width: 200 });
bounds.lines;
measured.width;
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

// Numeric tuples are fixed-length: colors are exactly RGBA and transforms are
// exactly 16 numbers, so arity mistakes are caught at compile time.
// @ts-expect-error — a color is exactly four components
efx.graphics.setClearColor([0, 0, 0]);
// @ts-expect-error — a transform is a fixed-length 16-number matrix
efx.graphics.drawMesh(cube, { transform: [1, 0, 0] });

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
ground; crate; hero; hit; hits;
// @ts-expect-error — the shape is positional, not a bag field
efx.physics.createBody({ shape: { type: 'sphere', radius: 1 } });
// @ts-expect-error — raycast requires a positional maxDistance
efx.physics.raycast([0, 1, 0], [1, 0, 0]);
// @ts-expect-error — createCharacter requires positional radius/height
efx.physics.createCharacter({ radius: 0.4, height: 1.8 });
