// F4b golden: an ambient map modulates the flat ambient term with every
// light disabled. ambient.color(white) × map(128,128,128) × albedo(white)
//       = (0.5, 0.5, 0.5) -> 8-bit (128, 128, 128)
efx.setClearColor([0, 0, 0, 1]);
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
const map = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [128, 128, 128, 255],
}));
const S = 20;
const plane = efx.createMesh(efx.createMeshData({
    positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0],
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
    uvs: [0, 0, 1, 0, 1, 1, 0, 1],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.setMeshSurfaceMaterial(plane, 0, {
    ambient:  { color: [1, 1, 1, 1], map: map },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.drawMesh(plane);
}
