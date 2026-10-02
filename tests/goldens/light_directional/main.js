// F4a golden: single directional light — the terminator follows -dir
efx.graphics.setClearColor([0.02, 0.03, 0.05, 1]);
efx.graphics.setCamera3D({ pos: [0, 0, 4.2], target: [0, 0, 0], fov: 55 });
efx.graphics.setDirectionalLight({ dir: [-0.6, -0.5, -0.6], color: [1.0, 0.95, 0.9, 1] });
const ball = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 1.1, segments: 32 }));
efx.graphics.setMeshSurfaceMaterial(ball, 0, {
    ambient:  { color: [0.04, 0.04, 0.06, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [0.3, 0.3, 0.3, 1], shininess: 24 },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.drawMesh(ball, { color: [0.85, 0.82, 0.75, 1] });
}
