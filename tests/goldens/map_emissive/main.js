// F4b golden: an emissive map scales the unmodulated emissive term with all
// lights disabled. emissive.color(white) × map(51,153,255)
//       = (0.2, 0.6, 1.0) -> 8-bit (51, 153, 255)
efx.setClearColor([0, 0, 0, 1]);
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
const map = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [51, 153, 255, 255],
}));
const S = 20;
const plane = efx.createMesh(efx.createMeshData({
    positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0],
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
    uvs: [0, 0, 1, 0, 1, 1, 0, 1],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.setMeshSurfaceMaterial(plane, 0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [1, 1, 1, 1], map: map },
});
function update() {}
function render() {
    efx.drawMesh(plane);
}
