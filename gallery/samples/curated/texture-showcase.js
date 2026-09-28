// A real CC0 paving-stone texture (ambientCG "PavingStones070", CC0 1.0)
// loaded from this sample's asset pack. It is shown full and cropped with
// sourceRect in the 2D HUD, and tiled across a 3D ground plane and a spinning
// cube through the texture's default repeat sampler.
efx.setClearColor([0.04, 0.05, 0.08, 1]);

// The 2D frame drives drawQuad and the 3D camera drives drawMesh; the renderer
// keeps both, so the HUD and the scene share one frame.
efx.setCamera2D({ frame: [640, 480] });
efx.setCamera3D({ pos: [3.4, 2.8, 4.6], target: [0, 0.3, 0], fov: 55 });

efx.setLight(0, { pos: [4.0, 6.0, 3.5], color: [1, 0.96, 0.9, 1], range: 40 });
efx.setDirectionalLight({ dir: [-0.4, -0.9, -0.4], color: [0.2, 0.22, 0.3, 1] });

const tex = efx.loadTexture('paving_color.jpg');

// Ground plane whose UVs run 0..6, so the repeat sampler tiles the texture.
const ground = efx.createMesh(efx.createMeshData({
    positions: [-4, 0, -4, 4, 0, -4, 4, 0, 4, -4, 0, 4],
    normals: [0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0],
    uvs: [0, 0, 6, 0, 6, 6, 0, 6],
    indices: [0, 1, 2, 0, 2, 3],
}));
efx.setMeshSurfaceMaterial(ground, 0, {
    ambient:  { color: [0.18, 0.18, 0.2, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: tex },
    specular: { color: [0.2, 0.2, 0.2, 1], shininess: 16 },
    emissive: { color: [0, 0, 0, 1] },
});

const cube = efx.createMesh(efx.makeCube({ size: 1.1 }));
efx.setMeshSurfaceMaterial(cube, 0, {
    ambient:  { color: [0.2, 0.2, 0.22, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: tex },
    specular: { color: [0.5, 0.5, 0.5, 1], shininess: 48 },
    emissive: { color: [0, 0, 0, 1] },
});

let t = 0;
function update(dt) { t += dt; }

function render() {
    efx.drawMesh({ mesh: ground });
    const spin = efx.mat4.rotate(efx.mat4.identity(), t * 35, [0, 1, 0]);
    efx.drawMesh({
        mesh: cube,
        transform: efx.mat4.translate(spin, [0, 0.55, 0]),
    });

    // 2D HUD: the texture full, then a sourceRect crop of its centre.
    efx.drawQuad(16, 16, tex, { size: [128, 128] });
    efx.drawQuad(160, 16, tex, {
        size: [128, 128],
        sourceRect: { x: 128, y: 128, w: 256, h: 256 },
    });
}
