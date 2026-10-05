// F4b golden: an emissive map scales the unmodulated emissive term with all
// lights disabled. emissive.color(white) × map(51,153,255)
//       = (0.2, 0.6, 1.0) -> 8-bit (51, 153, 255)
efx.graphics.setClearColor([0, 0, 0, 1]);
efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 55);
const map = efx.graphics.createTexture(efx.graphics.createImageData(1, 1, [51, 153, 255, 255]));
const S = 20;
const plane = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0], normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1], uvs: [0, 0, 1, 0, 1, 1, 0, 1], indices: [0, 1, 2, 0, 2, 3] }]));
plane.setSurfaceMaterial(0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [1, 1, 1, 1], map: map },
});
function update() {}
function render() {
    efx.graphics.drawMesh(plane);
}
