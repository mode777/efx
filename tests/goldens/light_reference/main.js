// F4a golden: the CPU-reference configuration (design D8). A camera-facing
// plane is lit head-on by a directional light with no specular/ambient/
// emissive, so every fragment is exactly material.diffuse × light color:
//   albedo = white tint, N = L = (0,0,1) -> ndl = 1
//   out = (1,1,1) albedo * directional color (0.8, 0.5, 0.2)
//       = (0.8, 0.5, 0.2)  -> 8-bit (204, 128, 51)
// efx_render_tests `lighting_reference` asserts these same analytic values
// for efx_lighting_shade; this committed capture pins the GPU agreement.
efx.graphics.setClearColor([0, 0, 0, 1]);
efx.graphics.setCamera3D({ pos: [0, 0, 5], target: [0, 0, 0], fov: 55 });
efx.graphics.setDirectionalLight({ dir: [0, 0, -1], color: [0.8, 0.5, 0.2, 1] });
const S = 20;
const plane = efx.graphics.createMesh(efx.graphics.createMeshData({
    positions: [-S, -S, 0, S, -S, 0, S, S, 0, -S, S, 0],
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.graphics.setMeshSurfaceMaterial(plane, 0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.drawMesh(plane);
}
