// A lit, spinning cube on a dark stage — the smallest complete 3D scene.
efx.setClearColor([0.03, 0.04, 0.09, 1]);
efx.setCamera3D({ pos: [0, 1.6, 4.2], target: [0, 0, 0], fov: 60 });
efx.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });

const cube = efx.createMesh(efx.makeCube({ size: 1.4 }));
efx.setMeshSurfaceMaterial(cube, 0, {
    ambient:  { color: [0.12, 0.12, 0.16, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
    emissive: { color: [0, 0, 0, 1] },
});

let t = 0;
function update(dt) { t += dt; }
function render() {
    const spin = efx.mat4.rotate(efx.mat4.identity(), t * 40, [0, 1, 0]);
    const tilt = efx.mat4.rotate(spin, 18, [1, 0, 0]);
    efx.drawMesh(cube, { transform: tilt, color: [0.95, 0.5, 0.2, 1] });
}
