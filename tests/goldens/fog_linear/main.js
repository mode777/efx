// Fog golden (linear): a ground plane receding from the camera ramps from
// no fog at `near` to full fog color at `far`; clear color matches the fog
// color so the far edge blends seamlessly into the background
efx.graphics.setClearColor([0.45, 0.55, 0.65, 1]);
efx.graphics.setCamera3D([0, 2.5, 10], [0, 0, -8], 55);
efx.graphics.setDirectionalLight({ dir: [0.3, -1, -0.2], color: [1.0, 0.98, 0.9, 1] });
efx.graphics.setFog({ color: [0.45, 0.55, 0.65, 1], near: 10, far: 45 });

// single quad, x in [-8, 8], z in [+6, -50]; up normals
const P = [
    -8, 0, 6,  -8, 0, -50,  8, 0, -50,
    -8, 0, 6,   8, 0, -50,  8, 0, 6,
];
const N = [
    0, 1, 0, 0, 1, 0, 0, 1, 0,
    0, 1, 0, 0, 1, 0, 0, 1, 0,
];
const ground = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: P, normals: N }]));
ground.setSurfaceMaterial(0, {
    ambient:  { color: [0.08, 0.09, 0.11, 1] },
    diffuse:  { color: [0.85, 0.82, 0.75, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.drawMesh(ground);
}
