// F4b golden: a diffuse map modulates the diffuse channel. A camera-facing
// plane with a uniform 1x1 diffuse map under a head-on white directional
// light: out = diffuse.color(white) × map(102,204,51) × albedo(white)
//                                        × light(white) × ndl(1)
//       = (0.4, 0.8, 0.2) -> 8-bit (102, 204, 51)
efx.graphics.setClearColor([0, 0, 0, 1]);
efx.graphics.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
efx.graphics.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const map = efx.graphics.createTexture(efx.graphics.createImageData({
    width: 1, height: 1, pixels: [102, 204, 51, 255],
}));
const S = 20;
const plane = efx.graphics.createMesh(efx.graphics.createMeshData({
    positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0],
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
    uvs: [0, 0, 1, 0, 1, 1, 0, 1],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.graphics.setMeshSurfaceMaterial(plane, 0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: map },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.drawMesh(plane);
}
