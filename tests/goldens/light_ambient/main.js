// F4a golden: ambient term is flat per surface (no lights enabled)
efx.setClearColor([0.02, 0.02, 0.03, 1]);
efx.setCamera3D({ pos: [0, 0, 4.2], target: [0, 0, 0], fov: 55 });
const red = efx.createMesh(efx.makeSphere({ radius: 0.85, segments: 24 }));
const blue = efx.createMesh(efx.makeSphere({ radius: 0.85, segments: 24 }));
efx.setMeshSurfaceMaterial(red, 0, {
    ambient:  { color: [0.65, 0.18, 0.18, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
efx.setMeshSurfaceMaterial(blue, 0, {
    ambient:  { color: [0.18, 0.38, 0.7, 1] },
    diffuse:  { color: [0, 0, 0, 1] },
    specular: { color: [0, 0, 0, 1] },
    emissive: { color: [0, 0, 0, 1] },
});
function update() {}
function render() {
    efx.drawMesh({ mesh: red,
        transform: efx.mat4.translate(efx.mat4.identity(), [-1.15, 0, 0]) });
    efx.drawMesh({ mesh: blue,
        transform: efx.mat4.translate(efx.mat4.identity(), [1.15, 0, 0]) });
}
