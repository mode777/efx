// F4a golden: vertex colors are the lit albedo (tint multiplies vertex color)
const MAT = {
    ambient:  { color: [0.14, 0.14, 0.16, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [0.6, 0.6, 0.6, 1], shininess: 24 },
    emissive: { color: [0, 0, 0, 1] },
};
efx.setClearColor([0.03, 0.07, 0.06, 1]);
efx.setCamera3D({ pos: [0, 0, 3], target: [0, 0, 0], fov: 55 });
efx.setLight(0, { pos: [1.5, 2.5, 2.5], color: [1, 0.95, 0.9, 1], range: 30 });
efx.setDirectionalLight({ dir: [-0.3, -0.6, -1.0], color: [0.2, 0.24, 0.3, 1] });
const P = [
    -1.1, -0.9, 0,
     1.1, -0.9, 0,
     0.0,  1.1, 0,
];
const C = [
    1, 0, 0, 1,
    0, 1, 0, 1,
    0, 0.3, 1, 1,
];
const tri = efx.createMesh(efx.createMeshData({
    positions: P,
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1],
    colors: C,
    indices: [0, 1, 2],
}));
efx.setMeshSurfaceMaterial(tri, 0, MAT);
const triTinted = efx.createMesh(efx.createMeshData({
    positions: P,
    normals: [0, 0, 1, 0, 0, 1, 0, 0, 1],
    colors: C,
    indices: [0, 1, 2],
}));
efx.setMeshSurfaceMaterial(triTinted, 0, MAT);
function update() {}
function render() {
    const right = efx.mat4.translate(efx.mat4.identity(), [-1.2, 0, 0]);
    efx.drawMesh({ mesh: tri, transform: right });                    // white tint
    const left = efx.mat4.translate(efx.mat4.identity(), [1.25, 0, 0]);
    efx.drawMesh({ mesh: triTinted, transform: left,
                   color: [1, 0.6, 0.5, 1] });                       // warm tint
}
