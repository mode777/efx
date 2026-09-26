// F4a smoke: argument validation of lights and Phong materials (portable —
// identical behavior on the desktop and web bindings; shading itself is
// covered by the unit/golden suites)
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

// setLight: valid + disabled by default + errors
efx.setLight(0, { pos: [1, 2, 3], color: [1, 0.5, 0.25, 1], range: 10 });
efx.setLight(1, { pos: [0, 1, 0], color: [1, 1, 1, 1] });
efx.setLight(1, null);
expectThrow('light-slot', RE, () => efx.setLight(4, { pos: [0, 0, 0], color: [1, 1, 1, 1] }));
expectThrow('light-slot-neg', RE, () => efx.setLight(-1, { pos: [0, 0, 0], color: [1, 1, 1, 1] }));
expectThrow('light-no-pos', TE, () => efx.setLight(0, { color: [1, 1, 1, 1] }));
expectThrow('light-no-color', TE, () => efx.setLight(0, { pos: [0, 0, 0] }));
expectThrow('light-pos-short', RE, () => efx.setLight(0, { pos: [0, 0], color: [1, 1, 1, 1] }));
expectThrow('light-range-type', TE, () => efx.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], range: 'far' }));
expectThrow('light-range-neg', RE, () => efx.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], range: -1 }));
expectThrow('light-unknown', TE, () => efx.setLight(0, { pos: [0, 0, 0], color: [1, 1, 1, 1], frobnicate: 1 }));

// setDirectionalLight: valid, null, errors
efx.setDirectionalLight({ dir: [-0.5, -1, -0.3], color: [0.2, 0.25, 0.35, 1] });
efx.setDirectionalLight(null);
expectThrow('dir-no-dir', TE, () => efx.setDirectionalLight({ color: [1, 1, 1, 1] }));
expectThrow('dir-zero', TE, () => efx.setDirectionalLight({ dir: [0, 0, 0], color: [1, 1, 1, 1] }));
expectThrow('dir-unknown', TE, () => efx.setDirectionalLight({ dir: [0, -1, 0], color: [1, 1, 1, 1], foo: 1 }));

// materials in createMeshData
const M = { diffuse: { color: [0.8, 0.3, 0.2, 1] }, specular: { color: [1, 1, 1, 1], shininess: 32 } };
const md = efx.createMeshData({
    surfaces: [{ positions: P, indices: [0, 1, 2] }, { positions: P, indices: [0, 1, 2] }],
    materials: [M, null],
});
if (md.surfaceCount !== 2) { efx.log('FAIL md materials count'); efx.quit(3); }
expectThrow('md-mat-length', RE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [] }));
expectThrow('md-mat-not-array', TE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: M }));
expectThrow('md-mat-bad-entry', TE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [42] }));

// material channel validation
expectThrow('mat-map-f4b', TE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [{ diffuse: { color: [1, 1, 1, 1], map: 1 } }] }));
expectThrow('mat-short-color', RE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [{ diffuse: { color: [1, 1, 1] } }] }));
expectThrow('mat-no-color', TE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [{ diffuse: {} }] }));
expectThrow('mat-unknown', TE, () => efx.createMeshData({ surfaces: [{ positions: P, indices: [0, 1, 2] }], materials: [{ diffuse: { color: [1, 1, 1, 1] }, albedo: 1 }] }));

// setMeshSurfaceMaterial: bind, rebind, null reset, errors
const mesh = efx.createMesh(md);
efx.setMeshSurfaceMaterial(mesh, 0, M);
efx.setMeshSurfaceMaterial(mesh, 0, { emissive: { color: [1, 0, 0, 1] } });
efx.setMeshSurfaceMaterial(mesh, 0, null);
efx.setMeshSurfaceMaterial(mesh, 1, M);
expectThrow('smsm-nonmesh', TE, () => efx.setMeshSurfaceMaterial({}, 0, M));
expectThrow('smsm-index', RE, () => efx.setMeshSurfaceMaterial(mesh, 2, M));
expectThrow('smsm-index-neg', RE, () => efx.setMeshSurfaceMaterial(mesh, -1, M));
expectThrow('smsm-bad-mat', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, 5));
expectThrow('smsm-shininess0', RE, () => efx.setMeshSurfaceMaterial(mesh, 0, { specular: { color: [1, 1, 1, 1], shininess: 0 } }));
mesh.destroy();
expectThrow('smsm-destroyed', TE, () => efx.setMeshSurfaceMaterial(mesh, 0, M));
md.destroy();

efx.log('s-4a-validation-ok');
