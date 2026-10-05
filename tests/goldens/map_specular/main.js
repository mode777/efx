// F4b golden: a specular map modulates the specular highlight color. A sphere
// with a black diffuse and a red specular map lit by a point light shows a
// red highlight instead of the default white one.
efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);
efx.graphics.setCamera3D([0, 0, 4], [0, 0, 0], 55);
efx.graphics.setLight(0, { pos: [2.0, 3.0, 3.0], color: [1, 1, 1, 1], range: 30 });
const specMap = efx.graphics.createTexture(efx.graphics.createImageData(1, 1, [255, 32, 32, 255]));
const ball = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 1.4, segments: 24 }));
efx.graphics.setMeshSurfaceMaterial(ball, 0, {
    ambient:  { color: [0, 0, 0, 1] },
    diffuse:  { color: [0.05, 0.05, 0.06, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32, map: specMap },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.drawMesh(ball);
}
