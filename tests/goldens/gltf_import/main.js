// F6b golden: glTF import (quad.glb) — a textured base-color surface with an
// alpha MASK material (left) plus an emissive solid material (right), driven
// by a head-on white directional light.
efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);
efx.graphics.setCamera3D([0, 0, 4], [0, 0, 0], 55);
efx.graphics.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const gltfMesh = efx.graphics.createMesh(efx.graphics.loadMeshData('quad.glb'));
function update() {}
function render() {
    efx.graphics.drawMesh(gltfMesh);
}
