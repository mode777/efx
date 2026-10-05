// F4a golden: per-surface materials — surface 0 explicitly bound, surface 1
// uses the engine default material (white diffuse Phong)
efx.graphics.setClearColor([0.06, 0.05, 0.09, 1]);
efx.graphics.setCamera3D([0, 0, 4.2], [0, 0, 0], 55);
efx.graphics.setLight(0, { pos: [2.0, 3.0, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.3, -0.6, -0.7], color: [0.18, 0.2, 0.28, 1] });
function card(x, hex) {
    return {
        positions: [x - 0.9, -0.9, 0, x + 0.9, -0.9, 0,
                    x + 0.9, 0.9, 0, x - 0.9, 0.9, 0],
        normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
        indices: [0, 1, 2, 0, 2, 3],
    };
}
const pair = efx.graphics.createMesh(efx.graphics.createMeshData([card(-1.05, 0), card(1.05, 0)], [
        { ambient: { color: [0.1, 0.1, 0.12, 1] },
          diffuse: { color: [0.85, 0.25, 0.2, 1] },
          specular: { color: [0.8, 0.8, 0.8, 1], shininess: 32 },
          emissive: { color: [0, 0, 0, 1] } },
        null, /* default material: white diffuse Phong */
    ]));
function update() {}
function render() {
    efx.graphics.drawMesh(pair);
}
