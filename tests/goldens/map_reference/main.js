// F4b golden: the CPU-reference configuration with a diffuse map. A
// camera-facing plane is lit head-on by a white directional light with no
// specular/ambient/emissive, so every fragment is exactly
// material.diffuse.color × map × light color:
//   albedo = white tint, N = L = (0,0,1) -> ndl = 1
//   out = (1,1,1) × map(204,128,51) × white = (0.8, 0.5, 0.2) -> (204,128,51)
// efx_render_tests `lighting_maps` asserts the same analytic values for
// efx_lighting_shade; this committed capture pins the GPU agreement.
efx.graphics.setClearColor([0, 0, 0, 1]);
efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 55);
efx.graphics.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const map = efx.graphics.createTexture(efx.graphics.createImageData(1, 1, [204, 128, 51, 255]));
const S = 20;
const plane = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0], normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1], uvs: [0, 0, 1, 0, 1, 1, 0, 1], indices: [0, 1, 2, 0, 2, 3] }]));
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
