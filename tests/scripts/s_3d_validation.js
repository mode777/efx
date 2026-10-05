// F3 smoke: argument validation of the 3D API (portable — identical
// behavior on the desktop and web bindings; draw/upload paths are covered
// by the unit/golden suites)
function expectThrow(name, kind, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof kind)) {
            efx.log('FAIL kind ' + name + ': ' + e); efx.quit(2);
        }
    }
}
const TE = TypeError, RE = RangeError;

// setCamera3D
expectThrow('cam3d-noargs', TE, () => efx.graphics.setCamera3D());
expectThrow('cam3d-no-pos', TE, () => efx.graphics.setCamera3D(undefined, [0, 0, 0], 60));
expectThrow('cam3d-fov-type', TE, () => efx.graphics.setCamera3D([0, 0, 1], [0, 0, 0], 'wide'));
expectThrow('cam3d-fov-inf', RE, () => efx.graphics.setCamera3D([0, 0, 1], [0, 0, 0], Infinity));
expectThrow('cam3d-unknown', TE, () => efx.graphics.setCamera3D([0, 0, 1], [0, 0, 0], 60, { frobnicate: 1 }));
expectThrow('cam3d-pos-short', RE, () => efx.graphics.setCamera3D([0, 0], [0, 0, 0], 60));

// createMeshData
const P = [0, 0, 0, 1, 0, 0, 0, 1, 0];
expectThrow('md-none', TE, () => efx.graphics.createMeshData([{  }]));
expectThrow('md-both', TE, () => efx.graphics.createMeshData([{ positions: P }]));
expectThrow('md-empty', RE, () => efx.graphics.createMeshData([]));
expectThrow('md-trunc', RE, () => efx.graphics.createMeshData([{ positions: [0, 0, 0] }]));
expectThrow('md-mult', RE, () => efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0] }]));
expectThrow('md-idx-oob', RE, () => efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 3] }]));
expectThrow('md-idx-partial', RE, () => efx.graphics.createMeshData([{ positions: P, indices: [0, 1] }]));
expectThrow('md-idx-frac', RE, () => efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2, 0] }]));
expectThrow('md-nonidx-div', RE, () => efx.graphics.createMeshData([{ positions: [0, 0, 0, 1, 0, 0] }]));
expectThrow('md-norm-short', RE, () => efx.graphics.createMeshData([{ positions: P, normals: [0, 0, 1] }]));
expectThrow('md-unknown', TE, () => efx.graphics.createMeshData([{ positions: P, pixles: 1 }]));
expectThrow('md-materials-mismatch', RE, () => efx.graphics.createMeshData([{ positions: P }], []));
expectThrow('md-elem-type', TE, () => efx.graphics.createMeshData([{ positions: ['a', 0, 0, 1, 0, 0, 0, 1, 0] }]));
expectThrow('md-elem-nan', RE, () => efx.graphics.createMeshData([{ positions: [NaN, 0, 0, 1, 0, 0, 0, 1, 0] }]));

// valid: batch + shorthand, surfaceCount
const md = efx.graphics.createMeshData([
        { positions: P, normals: [0, 0, 1, 0, 0, 1, 0, 0, 1], indices: [0, 1, 2] },
        { positions: P },
    ]);
if (md.surfaceCount !== 2) { efx.log('FAIL md surfaceCount'); efx.quit(3); }
const one = efx.graphics.createMeshData([{ positions: P, indices: [0, 1, 2] }]);
if (one.surfaceCount !== 1) { efx.log('FAIL shorthand surfaceCount'); efx.quit(3); }

// createMesh + lifecycle
const mesh = efx.graphics.createMesh(md);
if (mesh.surfaceCount !== 2) { efx.log('FAIL mesh surfaceCount'); efx.quit(3); }
md.destroy(); // Mesh is a copy
if (mesh.surfaceCount !== 2) { efx.log('FAIL mesh after source destroy'); efx.quit(3); }

// drawMesh validation: mesh is a required positional argument
expectThrow('dm-none', TE, () => efx.graphics.drawMesh());
expectThrow('dm-nonmesh', TE, () => efx.graphics.drawMesh({}));
expectThrow('dm-null-mesh', TE, () => efx.graphics.drawMesh(null));
expectThrow('dm-bag-nonobject', TE, () => efx.graphics.drawMesh(mesh, 5));
expectThrow('dm-mesh-in-bag', TE, () => efx.graphics.drawMesh(mesh, { mesh }));
expectThrow('dm-transform-short', RE, () => efx.graphics.drawMesh(mesh, { transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3] }));
expectThrow('dm-transform-type', TE, () => efx.graphics.drawMesh(mesh, { transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 'x'] }));
expectThrow('dm-color-short', RE, () => efx.graphics.drawMesh(mesh, { color: [1, 0, 1] }));
expectThrow('dm-unknown', TE, () => efx.graphics.drawMesh(mesh, { frobnicate: 1 }));

// valid draws record headless (uploads queue until a GPU surface exists)
efx.graphics.setCamera3D([0, 2, 5], [0, 0, 0], 60);
efx.graphics.drawMesh(mesh, { transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 1], color: [0.5, 0.25, 1, 1] });
efx.graphics.drawMesh(mesh);

mesh.destroy();
mesh.destroy(); // idempotent
expectThrow('dm-destroyed', TE, () => efx.graphics.drawMesh(mesh));
expectThrow('sc-destroyed', TE, () => mesh.surfaceCount);
one.destroy();
expectThrow('md-destroyed', TE, () => one.surfaceCount);

efx.log('s-3d-validation-ok');
