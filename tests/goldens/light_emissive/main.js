// F4a golden: emissive is added unmodulated by albedo and tint
efx.graphics.setClearColor([0.01, 0.01, 0.02, 1]);
efx.graphics.setCamera3D([0, 0, 4.2], [0, 0, 0], 55);
const ball = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 1.1, segments: 32 }));
ball.setSurfaceMaterial(0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0.15, 0.75, 0.35, 1] },
});
function update() {}
function render() {
    // a non-white tint must not change the emissive term
    efx.graphics.drawMesh(ball, { color: [0.1, 0.1, 0.1, 1] });
}
