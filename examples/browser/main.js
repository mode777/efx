// Standalone demo bundled with the raw web player (`player_web.html`).
//
// The public sample gallery — the GitHub Pages site with the catalog, the
// editable Monaco editor, and the API type document — lives in `gallery/`.
// This file only keeps the downloadable web-player bundle runnable; it is no
// longer the gallery and does not mirror the golden scenes (the gallery
// generates its catalog from `tests/goldens/`).
efx.setClearColor([0.03, 0.04, 0.09, 1]);
efx.setCamera3D({ pos: [0, 1.6, 4.4], target: [0, 0, 0], fov: 60 });
efx.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });

const cube = efx.createMesh(efx.makeCube({ size: 1.5 }));
efx.setMeshSurfaceMaterial(cube, 0, {
    ambient:  { color: [0.12, 0.12, 0.16, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
    emissive: { color: [0, 0, 0, 1] },
});

let t = 0;
function update(dt) { t += dt; }
function render() {
    const spin = efx.mat4.rotate(efx.mat4.identity(), t * 35, [0, 1, 0]);
    efx.drawMesh(cube, {
        transform: efx.mat4.rotate(spin, 18, [1, 0, 0]),
        color: [0.95, 0.5, 0.2, 1],
    });
}
