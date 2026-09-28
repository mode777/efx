// F4b smoke: per-channel maps + alphaMask parsing, validation, and the
// retained-map lifetime contract (portable — identical on desktop and web;
// the rendered result is covered by the unit/golden suites).
function expectThrow(name, kind, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof kind)) {
            efx.log('FAIL kind ' + name + ': ' + e); efx.quit(2);
        }
    }
}
const TE = TypeError, RE = RangeError;
const P = [0, 0, 0, 1, 0, 0, 0, 1, 0];
const UV = [0, 0, 1, 0, 0, 1];

const tex = efx.createTexture(efx.createImageData({
    width: 2, height: 2,
    pixels: new Uint8Array([255, 0, 0, 255, 0, 255, 0, 255,
                            0, 0, 255, 255, 255, 255, 255, 0]),
}));

// valid maps + alphaMask on every channel
const M = {
    ambient:  { color: [0.1, 0.1, 0.1, 1], map: tex },
    diffuse:  { color: [0.8, 0.3, 0.2, 1], map: tex },
    specular: { color: [1, 1, 1, 1], shininess: 48, map: tex },
    emissive: { color: [0, 0, 0, 1], map: tex },
    alphaMask: tex,
};
const md = efx.createMeshData({
    surfaces: [{ positions: P, uvs: UV, indices: [0, 1, 2] }],
    materials: [M],
});
if (md.surfaceCount !== 1) { efx.log('FAIL surface count'); efx.quit(3); }
const mesh = efx.createMesh(md);

// binding, then script-object mutation must not change the binding
efx.setMeshSurfaceMaterial(mesh, 0, M);
M.diffuse.map = null;
M.alphaMask = null;
efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: tex } });
efx.setMeshSurfaceMaterial(mesh, 0, null);

// alphaMask alone is valid; null map is treated as absent
efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: null }, alphaMask: tex });

// non-Texture / destroyed-Texture maps and unknown fields throw
expectThrow('map-number', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: 1 } }));
expectThrow('map-object', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: {} } }));
expectThrow('alphaMask-number', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, { alphaMask: 5 }));
expectThrow('channel-unknown', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], frob: 1 } }));
expectThrow('material-unknown', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, { albedo: 1 }));
expectThrow('md-map-length', RE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [] }));

// a texture destroyed while bound keeps shading; a freshly bound dead
// texture throws TypeError
efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: tex }, alphaMask: tex });
tex.destroy();
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 60 });
efx.drawMesh(mesh);
expectThrow('map-destroyed', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1], map: tex } }));
// rebind without the map releases it
efx.setMeshSurfaceMaterial(mesh, 0, { diffuse: { color: [1, 1, 1, 1] } });

mesh.destroy();
md.destroy();
efx.log('s-4b-validation-ok');
