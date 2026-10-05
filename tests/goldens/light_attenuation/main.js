// F4a golden: point-light range attenuation — red reaches far, blue falls
// off quickly; clamped to zero at each light's range
efx.graphics.setClearColor([0.01, 0.01, 0.02, 1]);
efx.graphics.setCamera3D([0, 0, 6], [0, 0, 0], 55);
efx.graphics.setLight(0, { pos: [-2.0, 0, 2.0], color: [1.0, 0.2, 0.15, 1], range: 7.5 });
efx.graphics.setLight(1, { pos: [2.0, 0, 2.0], color: [0.2, 0.4, 1.0, 1], range: 2.6 });
const P = [
    -3.0, -2.0, 0,  3.0, -2.0, 0,  3.0, 2.0, 0,
    -3.0, -2.0, 0,  3.0,  2.0, 0, -3.0, 2.0, 0,
];
const N = [
    0, 0, 1, 0, 0, 1, 0, 0, 1,
    0, 0, 1, 0, 0, 1, 0, 0, 1,
];
const slab = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: P, normals: N }]));
slab.setSurfaceMaterial(0, {
    ambient:  { color: [0.02, 0.02, 0.03, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.drawMesh(slab);
}
