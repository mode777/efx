// image-heightfield golden: terrain meshes from image luminance — the
// full-image default field (textured with the heightmap itself, proving the
// full-image uv mapping) and a region+step crop (plain material) beside it
const img = efx.graphics.loadImage('heightmap.png');
const tex = efx.graphics.createTexture(img);
const FIELD_MAT = {
    ambient:  { color: [0.14, 0.14, 0.16, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: tex },
    specular: { color: [0.1, 0.1, 0.1, 1], shininess: 8 },
    emissive: { color: [0, 0, 0, 1] },
};
const CROP_MAT = {
    ambient:  { color: [0.14, 0.14, 0.16, 1] },
    diffuse:  { color: [0.35, 0.55, 0.9, 1] },
    specular: { color: [0.1, 0.1, 0.1, 1], shininess: 8 },
    emissive: { color: [0, 0, 0, 1] },
};
const field = efx.graphics.createMesh(
    efx.graphics.makeHeightfield(img, { height: 4, size: 12, material: FIELD_MAT }));
const crop = efx.graphics.createMesh(efx.graphics.makeHeightfield(img, {
    region: [16, 16, 32, 32],
    step: 2,
    height: 4,
    size: 6,
    material: CROP_MAT,
}));
efx.graphics.setClearColor([0.07, 0.07, 0.12, 1]);
efx.graphics.setCamera3D([10, 9, 12], [-1, 1, 0], 55);
efx.graphics.setDirectionalLight({ dir: [-0.45, -1, -0.3], color: [0.95, 0.92, 0.85, 1] });
const fieldXf = efx.math.mat4.translate(efx.math.mat4.identity(), -4.5, 0, 0);
const cropXf = efx.math.mat4.translate(efx.math.mat4.identity(), 5, 0, 0);
function update() {}
function render() {
    efx.graphics.drawMesh(field, { transform: fieldXf });
    efx.graphics.drawMesh(crop, { transform: cropXf });
}
