// F4a golden: transform variety — translate, rotate, scale compose (lit)
const MAT = {
    ambient:  { color: [0.12, 0.12, 0.14, 1] },
    diffuse:  { color: [1, 1, 1, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 24 },
    emissive: { color: [0, 0, 0, 1] },
};
efx.graphics.setClearColor([0.06, 0.06, 0.06, 1]);
efx.graphics.setCamera3D([0, 2.2, 5.2], [0, 0, 0], 55);
efx.graphics.setLight(0, { pos: [3.0, 4.0, 3.5], color: [1, 0.95, 0.9, 1], range: 35 });
efx.graphics.setDirectionalLight({ dir: [-0.3, -0.8, -0.5], color: [0.2, 0.22, 0.28, 1] });
const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 0.9 }));
cube.setSurfaceMaterial(0, MAT);
function update() {}
function render() {
    // translate only
    efx.graphics.drawMesh(cube, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [-1.9, 0, 0]),
        color: [0.9, 0.3, 0.3, 1] });
    // translate + rotate 45 about Y
    const rot = efx.math.mat4.rotate(efx.math.mat4.identity(), 45, [0, 1, 0]);
    efx.graphics.drawMesh(cube, {
        transform: efx.math.mat4.translate(rot, [0, 0, 0]),
        color: [0.3, 0.9, 0.4, 1] });
    // translate + rotate + scale 1.8
    const sc = efx.math.mat4.scale(rot, [1.8, 1.8, 1.8]);
    efx.graphics.drawMesh(cube, {
        transform: efx.math.mat4.translate(sc, [1.9, 0, 0]),
        color: [0.35, 0.45, 0.95, 1] });
}
