// F4a golden: one mesh, two surfaces — each surface has its own material,
// drawn in surface order (depth orders the overlap)
const MAT0 = {
    ambient:  { color: [0.10, 0.12, 0.16, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 24 },
    emissive: { color: [0, 0, 0, 1] },
};
const MAT1 = {
    ambient:  { color: [0.16, 0.12, 0.10, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 48 },
    emissive: { color: [0, 0, 0, 1] },
};
efx.graphics.setClearColor([0.08, 0.05, 0.11, 1]);
efx.graphics.setCamera3D({ pos: [0, 0, 4], target: [0, 0, 0], fov: 55 });
efx.graphics.setLight(0, { pos: [2.0, 2.5, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.3, -0.5, -0.8], color: [0.2, 0.24, 0.32, 1] });
function card(z, s, rgb) {
    return {
        positions: [-s, -s, z, s, -s, z, s, s, z, -s, s, z],
        normals: [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1],
        colors: [rgb[0], rgb[1], rgb[2], 1, rgb[0], rgb[1], rgb[2], 1,
                 rgb[0], rgb[1], rgb[2], 1, rgb[0], rgb[1], rgb[2], 1],
        indices: [0, 1, 2, 0, 2, 3],
    };
}
const two = efx.graphics.createMesh(efx.graphics.createMeshData({
    surfaces: [
        card(0, 1.0, [0.2, 0.55, 0.95]),   // surface 0: far, blue
        card(1.2, 0.55, [1.0, 0.6, 0.1]),  // surface 1: near, orange
    ],
    materials: [MAT0, MAT1],
}));
function update() {}
function render() {
    efx.graphics.drawMesh(two);
}
