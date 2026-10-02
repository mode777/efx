// F4a golden: procedural cube lit by point + directional light (primitives,
// transform, Phong material); re-baselined from the F3 unlit fill
const MAT = {
    ambient:  { color: [0.12, 0.12, 0.14, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 24 },
    emissive: { color: [0, 0, 0, 1] },
};
efx.graphics.setClearColor([0.05, 0.05, 0.1, 1]);
efx.graphics.setCamera3D({ pos: [0, 1.6, 4.2], target: [0, 0, 0], fov: 60 });
efx.graphics.setLight(0, { pos: [2.5, 3.5, 3.0], color: [1.0, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });
const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.4 }));
efx.graphics.setMeshSurfaceMaterial(cube, 0, MAT);
const yaw = efx.mat4.rotate(efx.mat4.identity(), 35, [0, 1, 0]);
const pitch = efx.mat4.rotate(yaw, 22, [1, 0, 0]);
function update() {}
function render() {
    efx.graphics.drawMesh(cube, {
        transform: pitch,
        color: [0.95, 0.45, 0.15, 1],
    });
}
