// A real CC0 glTF model — "Avocado" by Microsoft, from the Khronos
// glTF-Sample-Assets (CC0 1.0) — imported with loadMeshData. Its base-color
// map and PBR-to-Phong material are bound per surface. The optimized asset
// drops the normal/occlusion/metallic-roughness maps the importer ignores and
// picks matte factors, so it reads well under the fixed-function lighting.
efx.setClearColor([0.04, 0.05, 0.09, 1]);
efx.setCamera3D({ pos: [0, 0.35, 1.15], target: [0, 0.25, 0], fov: 50 });
efx.setLight(0, { pos: [1.2, 1.6, 1.4], color: [1, 0.96, 0.9, 1], range: 12 });
efx.setDirectionalLight({ dir: [-0.4, -0.8, -0.5], color: [0.22, 0.24, 0.3, 1] });

const avocado = efx.createMesh(efx.loadMeshData('Avocado.gltf'));

let t = 0;
function update(dt) { t += dt; }

function render() {
    const spin = efx.mat4.rotate(efx.mat4.identity(), t * 30, [0, 1, 0]);
    efx.drawMesh({ mesh: avocado, transform: efx.mat4.scale(spin, [8, 8, 8]) });
}
