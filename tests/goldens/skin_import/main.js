// F6c golden: rest-pose render of a skinned glTF asset (skin.gltf) — a quad
// bound to a two-joint skeleton, lit by a white directional light. F6c does
// not pose, so this proves the imported rig leaves the bind pose intact.
efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);
efx.graphics.setCamera3D([0, 0, 4], [0, 0, 0], 55);
efx.graphics.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const skinned = efx.graphics.createMesh(efx.graphics.loadMeshData('skin.gltf'));
function update() {}
function render() {
    efx.graphics.drawMesh(skinned);
}
