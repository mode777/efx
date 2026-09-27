// F6b golden: glTF import (quad.glb) — a textured base-color surface with an
// alpha MASK material (left) plus an emissive solid material (right), driven
// by a head-on white directional light.
efx.setClearColor([0.05, 0.05, 0.08, 1]);
efx.setCamera3D({ pos: [0, 0, 4], target: [0, 0, 0], fov: 55 });
efx.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const gltfMesh = efx.createMesh(efx.loadMeshData('quad.glb'));
function update() {}
function render() {
    efx.drawMesh({ mesh: gltfMesh });
}
