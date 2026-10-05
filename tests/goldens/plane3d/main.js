// F4a golden: segmented procedural plane tilted over a lit sphere
const MATP = {
    ambient:  { color: [0.12, 0.13, 0.16, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [0.4, 0.4, 0.4, 1], shininess: 16 },
    emissive: { color: [0, 0, 0, 1] },
};
const MATB = {
    ambient:  { color: [0.12, 0.12, 0.12, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 48 },
    emissive: { color: [0, 0, 0, 1] },
};
efx.graphics.setClearColor([0.02, 0.05, 0.08, 1]);
efx.graphics.setCamera3D([0, 2.6, 4.4], [0, -0.2, 0], 55);
efx.graphics.setLight(0, { pos: [3.0, 4.0, 2.5], color: [1, 0.96, 0.9, 1], range: 35 });
efx.graphics.setDirectionalLight({ dir: [-0.3, -1.0, -0.2], color: [0.18, 0.22, 0.3, 1] });
const plane = efx.graphics.createMesh(efx.graphics.makePlane({ size: 4, segments: 4 }));
const ball = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 0.7, segments: 24 }));
plane.setSurfaceMaterial(0, MATP);
ball.setSurfaceMaterial(0, MATB);
const tilt = efx.math.mat4.rotate(efx.math.mat4.identity(), 12, [1, 0, 0]);
function update() {}
function render() {
    efx.graphics.drawMesh(plane, { transform: tilt,
                   color: [0.55, 0.62, 0.75, 1] });
    const up = efx.math.mat4.translate(efx.math.mat4.identity(), [0, 0.7, 0]);
    efx.graphics.drawMesh(ball, { transform: up,
                   color: [0.95, 0.8, 0.25, 1] });
}
