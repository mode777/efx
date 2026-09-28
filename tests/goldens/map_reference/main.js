// F4b golden: the CPU-reference configuration with a diffuse map. A
// camera-facing plane is lit head-on by a white directional light with no
// specular/ambient/emissive, so every fragment is exactly
// material.diffuse.color × map × light color:
//   albedo = white tint, N = L = (0,0,1) -> ndl = 1
//   out = (1,1,1) × map(204,128,51) × white = (0.8, 0.5, 0.2) -> (204,128,51)
// efx_render_tests `lighting_maps` asserts the same analytic values for
// efx_lighting_shade; this committed capture pins the GPU agreement.
efx.setClearColor([0, 0, 0, 1]);
efx.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
efx.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const map = efx.createTexture(efx.createImageData({
    width: 1, height: 1, pixels: [204, 128, 51, 255],
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
    diffuse:  { color: [1, 1, 1, 1], map: map },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.drawMesh(plane);
}
