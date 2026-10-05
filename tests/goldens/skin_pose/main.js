// F7 golden: a deterministic CPU pose of the skinned skin.gltf quad drawn with
// `skinned: true` — the "move" translation and the "turn" rotation blend to
// deform the bind pose while the bind buffer stays untouched.
efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);
efx.graphics.setCamera3D([0, 0, 4], [0, 0, 0], 55);
efx.graphics.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const skinned = efx.graphics.createMesh(efx.graphics.loadMeshData('skin.gltf'));
function update() {}
function render() {
    efx.graphics.poseMesh(skinned, [
        { clip: 'move', time: 0.5, weight: 1 },
        { clip: 'turn', time: 0.75, weight: 1 },
    ]);
    efx.graphics.drawMesh(skinned, { skinned: true });
}
