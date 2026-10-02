// F4a golden: depth test — the NEARER cube recorded first wins the overlap
const MAT = {
    ambient:  { color: [0.12, 0.12, 0.15, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
    emissive: { color: [0, 0, 0, 1] },
};
efx.graphics.setClearColor([0.04, 0.06, 0.09, 1]);
efx.graphics.setCamera3D({ pos: [0, 1.4, 4.5], target: [0, 0, 0.6], fov: 55 });
efx.graphics.setLight(0, { pos: [2.5, 3.5, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.2, 0.24, 0.3, 1] });
const near = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.1 }));
const far = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.1 }));
efx.graphics.setMeshSurfaceMaterial(near, 0, MAT);
efx.graphics.setMeshSurfaceMaterial(far, 0, MAT);
function update() {}
function render() {
    // nearer (z = 1.6) recorded first
    efx.graphics.drawMesh(near, {
        transform: efx.mat4.translate(efx.mat4.identity(), [0.35, 0, 1.6]),
        color: [0.95, 0.35, 0.25, 1] });
    // farther (origin) recorded second, overlapping on screen
    efx.graphics.drawMesh(far, {
        transform: efx.mat4.translate(efx.mat4.identity(), [-0.35, 0, 0]),
        color: [0.25, 0.8, 0.45, 1] });
}
