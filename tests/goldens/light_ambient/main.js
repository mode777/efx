// F4a golden: ambient term is flat per surface (no lights enabled)
efx.graphics.setClearColor([0.02, 0.02, 0.03, 1]);
efx.graphics.setCamera3D({ pos: [0, 0, 4.2], target: [0, 0, 0], fov: 55 });
const red = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 0.85, segments: 24 }));
const blue = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 0.85, segments: 24 }));
efx.graphics.setMeshSurfaceMaterial(red, 0, {
    ambient:  { color: [0.65, 0.18, 0.18, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
efx.graphics.setMeshSurfaceMaterial(blue, 0, {
    ambient:  { color: [0.18, 0.38, 0.7, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.graphics.drawMesh(red, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [-1.15, 0, 0]) });
    efx.graphics.drawMesh(blue, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [1.15, 0, 0]) });
}
