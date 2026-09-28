// F7 golden: a deterministic CPU pose of the skinned skin.gltf quad drawn with
// `skinned: true` — the "move" translation and the "turn" rotation blend to
// deform the bind pose while the bind buffer stays untouched.
efx.setClearColor([0.05, 0.05, 0.08, 1]);
efx.setCamera3D({ pos: [0, 0, 4], target: [0, 0, 0], fov: 55 });
efx.setDirectionalLight({ dir: [0, 0, -1], color: [1, 1, 1, 1] });
const skinned = efx.createMesh(efx.loadMeshData('skin.gltf'));
function update() {}
function render() {
    efx.poseMesh(skinned, [
        { clip: 'move', time: 0.5, weight: 1 },
        { clip: 'turn', time: 0.75, weight: 1 },
    ]);
    efx.drawMesh(skinned, { skinned: true });
}
