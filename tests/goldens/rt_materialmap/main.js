// F5a golden: a render target used as a Phong material map. A half-red /
// half-blue pattern is rendered into an 8x8 target, then bound as the
// diffuse map of a camera-facing plane under a head-on white directional
// light: out = diffuse.color(white) x map(x light x ndl(1)) — the left
// half of the plane shades red, the right half blue.
efx.setClearColor([0, 0, 0, 1]);
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
efx.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const red = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [255, 0, 0, 255],
}));
const blue = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [40, 80, 255, 255],
}));
const map = efx.createRenderTarget({ width: 8, height: 8 });
const S = 20;
const plane = efx.createMesh(efx.createMeshData({
    positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0],
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
    uvs: [0, 0, 1, 0, 1, 1, 0, 1],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.setMeshSurfaceMaterial(plane, 0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: map },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.beginRenderTarget(map);
    efx.drawQuad(0, 0, red, { size: [4, 8] });
    efx.drawQuad(4, 0, blue, { size: [4, 8] });
    efx.endRenderTarget();
    efx.drawMesh({ mesh: plane });
}
