// A lit, spinning cube on a dark stage — the smallest complete 3D scene.
efx.graphics.setClearColor([0.03, 0.04, 0.09, 1]);
efx.graphics.setCamera3D([0, 1.6, 4.2], [0, 0, 0], 60);
efx.graphics.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });

const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.4 }));
cube.setSurfaceMaterial(0, {
    ambient:  { color: [0.12, 0.12, 0.16, 1] },
    diffuse:  { color: efx.color.white },
    specular: { color: efx.color.white, shininess: 32 },
    emissive: { color: efx.color.black },
});

let t = 0;
function update(dt) { t += dt; }
function render() {
    const spin = efx.math.mat4.rotate(efx.math.mat4.identity(), t * 40, [0, 1, 0]);
    const tilt = efx.math.mat4.rotate(spin, 18, [1, 0, 0]);
    efx.graphics.drawMesh(cube, { transform: tilt, color: [0.95, 0.5, 0.2, 1] });
}
