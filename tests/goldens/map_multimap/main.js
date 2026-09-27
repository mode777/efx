// F4b golden: all four channel maps combine on one surface. A camera-facing
// plane under a head-on white directional light plus flat ambient; each
// channel is scaled by its map:
//   ambient(1)×ambMap + diffuse(1)×difMap×light(1) + specular(1)×specMap
//   + emissive(1)×emiMap
// with ambMap=(0.2,0,0) difMap=(0,0.4,0) specMap=(0,0,0.3) emiMap=(0.1,0.1,0.1)
//   = (0.3, 0.5, 0.4) -> 8-bit (77, 128, 102)
efx.setClearColor([0, 0, 0, 1]);
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
efx.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
function tex(r, g, b) {
    return efx.createTexture(efx.createImageData({
        width: 1, height: 1, pixels: [r, g, b, 255],
    }));
}
const ambMap = tex(51, 0, 0);
const difMap = tex(0, 102, 0);
const specMap = tex(0, 0, 77);
const emiMap = tex(26, 26, 26);
const S = 20;
const plane = efx.createMesh(efx.createMeshData({
    positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0],
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
    uvs: [0, 0, 1, 0, 1, 1, 0, 1],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.setMeshSurfaceMaterial(plane, 0, {
    ambient:  { color: [1, 1, 1, 1], map: ambMap },
    diffuse:  { color: [1, 1, 1, 1], map: difMap },
    specular: { color: [1, 1, 1, 1], shininess: 32, map: specMap },
    emissive: { color: [1, 1, 1, 1], map: emiMap },
});
function update() {}
function render() {
    efx.drawMesh({ mesh: plane });
}
