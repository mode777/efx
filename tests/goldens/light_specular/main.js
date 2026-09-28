// F4a golden: Blinn-Phong specular highlight from a point light
efx.setClearColor([0.01, 0.01, 0.02, 1]);
efx.setCamera3D({ pos: [0, 0, 4.2], target: [0, 0, 0], fov: 55 });
efx.setLight(0, { pos: [0.6, 2.2, 3.0], color: [1, 1, 1, 1] });
const ball = efx.createMesh(efx.makeSphere({ radius: 1.15, segments: 32 }));
efx.setMeshSurfaceMaterial(ball, 0, {
    ambient:  { color: [0.01, 0.01, 0.01, 1] },
    diffuse:  { color: [0.06, 0.07, 0.1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 64 },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.drawMesh(ball);
}
