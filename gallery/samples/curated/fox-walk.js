// A real CC0 rigged model — "Fox" from Quaternius' Ultimate Animated Animals
// pack (CC0 1.0) — imported with loadMeshData and CPU-posed with poseMesh from
// a script-owned clock. The "Walk" clip drives the pose, drawn with
// `skinned: true`; the retained bind pose stays untouched.
efx.setClearColor([0.05, 0.06, 0.09, 1]);
efx.setCamera3D({ pos: [0, 1.6, 4.2], target: [0, 1.4, 0], fov: 45 });
efx.setDirectionalLight({ dir: [-0.4, -0.8, -0.5], color: [0.9, 0.9, 0.95, 1] });
efx.setLight(0, { pos: [2, 3, 3], color: [1, 0.95, 0.85, 1], range: 14 });

const fox = efx.createMesh(efx.loadMeshData('Fox.glb'));

// The mesh is authored in a small FBX space under a 100x, Z-up node; apply
// that node's transform so the walk reads upright at world scale.
const model = efx.mat4.scale(efx.mat4.rotate(efx.mat4.identity(), -90, [1, 0, 0]),
                             [100, 100, 100]);

let t = 0;
function update(dt) { t += dt; }

function render() {
    efx.poseMesh(fox, { clip: 'AnimalArmature|Walk', time: t });
    efx.drawMesh(fox, { transform: model, skinned: true });
}
