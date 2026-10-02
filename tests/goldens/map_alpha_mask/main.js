// F4b golden: an alpha mask is a binary cutout. Two identical ambient-lit
// cards; the left binds an opaque mask (alpha 255, renders), the right a
// transparent mask (alpha 0, discarded so the clear color shows through).
efx.graphics.setClearColor([0.1, 0.02, 0.2, 1]);
efx.graphics.setCamera3D({ pos: [0, 0, 4.5], target: [0, 0, 0], fov: 55 });
const opaque = efx.graphics.createTexture(efx.graphics.createImageData({
    width: 1, height: 1, pixels: [255, 255, 255, 255],
}));
const clear = efx.graphics.createTexture(efx.graphics.createImageData({
    width: 1, height: 1, pixels: [255, 255, 255, 0],
}));
function card(x) {
    return {
        positions: [x - 0.9, -0.9, 0, x + 0.9, -0.9, 0, x + 0.9, 0.9, 0,
                    x - 0.9, 0.9, 0],
        normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
        uvs: [0, 0, 1, 0, 1, 1, 0, 1],
        indices: [0, 1, 2, 0, 2, 3],
    };
}
const pair = efx.graphics.createMesh(efx.graphics.createMeshData({
    surfaces: [card(-1.05), card(1.05)],
    materials: [
        { ambient: { color: [1, 0.5, 0.2, 1] }, alphaMask: opaque },
        { ambient: { color: [1, 0.5, 0.2, 1] }, alphaMask: clear },
    ],
}));
function update() {}
function render() {
    efx.graphics.drawMesh(pair);
}
