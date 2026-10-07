// F14+ golden: an unlit material, an inverted primitive, and depthWrite:false.
// An inverted cube (a sky) carries an unlit procedural checker map and is
// recorded first with depthWrite:false. A lit cube placed BEYOND the sky's far
// face still renders (the sky wrote no depth), and a nearer lit cube draws
// over the sky. No third-party asset.
efx.graphics.setClearColor([0.02, 0.02, 0.05, 1]);
efx.graphics.setCamera3D([0, 0, 0], [0, 0, -1], 60, { near: 0.1, far: 100 });

// deterministic 8x8 checker (unlit path: texture is shown as authored)
const N = 8;
const px = new Uint8Array(N * N * 4);
for (let y = 0; y < N; y++) {
    for (let x = 0; x < N; x++) {
        const i = (y * N + x) * 4;
        const on = ((x + y) & 1) === 0;
        px[i] = on ? 40 : 180;
        px[i + 1] = on ? 80 : 160;
        px[i + 2] = on ? 200 : 40;
        px[i + 3] = 255;
    }
}
const skyTex = efx.graphics.createTexture(
    efx.graphics.createImageData(N, N, px), { wrap: 'clamp' });

// inward-facing cube: the inside is front-facing under BACK/CCW culling
const sky = efx.graphics.createMesh(
    efx.graphics.makeCube({ size: 20, inverted: true }));
sky.setSurfaceMaterial(0, {
    unlit: true,
    diffuse: { color: [1, 1, 1, 1], map: skyTex },
});

// a lit cube beyond the sky's far face (z=-12, sky half-extent 10)
const far = efx.graphics.createMesh(efx.graphics.makeCube({ size: 2 }));
far.setSurfaceMaterial(0, {
    diffuse: { color: [0.9, 0.5, 0.2, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
});

// a lit cube in front of the sky
const near = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.5 }));
near.setSurfaceMaterial(0, { diffuse: { color: [0.2, 0.7, 0.9, 1] } });

efx.graphics.setDirectionalLight({ dir: [0.3, -0.5, 0.8], color: [1, 1, 1, 1] });

function update() {}

function render() {
    efx.graphics.drawMesh(sky, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [0, 0, 0]),
        depthWrite: false,
    });
    efx.graphics.drawMesh(far, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [0, 0, -12]),
    });
    efx.graphics.drawMesh(near, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [0, 0, -3]),
    });
}
