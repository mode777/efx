// F5a golden: a render target used as a Phong material map. A half-red /
// half-blue pattern is rendered into an 8x8 target, then bound as the
// diffuse map of a camera-facing plane under a head-on white directional
// light: out = diffuse.color(white) x map(x light x ndl(1)) — the left
// half of the plane shades red, the right half blue.
efx.graphics.setClearColor([0, 0, 0, 1]);
efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 55);
efx.graphics.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const red = efx.graphics.createTexture(efx.graphics.createImageData(1, 1, [255, 0, 0, 255]));
const blue = efx.graphics.createTexture(efx.graphics.createImageData(1, 1, [40, 80, 255, 255]));
const map = efx.graphics.createRenderTarget(8, 8);
const S = 20;
const plane = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0], normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1], uvs: [0, 0, 1, 0, 1, 1, 0, 1], indices: [0, 1, 2, 0, 2, 3] }]));
plane.setSurfaceMaterial(0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: map },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.beginRenderTarget(map);
    efx.graphics.drawQuad(red, 0, 0, { size: [4, 8] });
    efx.graphics.drawQuad(blue, 4, 0, { size: [4, 8] });
    efx.graphics.endRenderTarget();
    efx.graphics.drawMesh(plane);
}
