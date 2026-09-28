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
