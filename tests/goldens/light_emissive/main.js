// F4a golden: emissive is added unmodulated by albedo and tint
efx.setClearColor([0.01, 0.01, 0.02, 1]);
efx.setCamera3D({ pos: [0, 0, 4.2], target: [0, 0, 0], fov: 55 });
const ball = efx.createMesh(efx.makeSphere({ radius: 1.1, segments: 32 }));
efx.setMeshSurfaceMaterial(ball, 0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0.15, 0.75, 0.35, 1] },
});
function update() {}
function render() {
    // a non-white tint must not change the emissive term
    efx.drawMesh(ball, { color: [0.1, 0.1, 0.1, 1] });
}
