// F14+ golden: an unlit material, an inverted primitive, and depthWrite:false.
// An inverted cube (a sky) carries an unlit sky texture and is recorded first
// with depthWrite:false. A lit cube placed BEYOND the sky's far face (z=-12,
// sky half-extent 10) still renders because the sky wrote no depth; a nearer
// lit cube draws over the sky. No third-party asset (the sky is procedural).
efx.graphics.setClearColor([0.02, 0.02, 0.05, 1]);
efx.graphics.setCamera3D([0, 0, 0], [0, 0, -1], 60, { near: 0.1, far: 100 });

// deterministic low-frequency vertical sky gradient (unlit: shown as authored)
const W = 4, H = 64;
const px = new Uint8Array(W * H * 4);
for (let y = 0; y < H; y++) {
    const f = y / (H - 1);
    for (let x = 0; x < W; x++) {
        const i = (y * W + x) * 4;
        px[i] = Math.round(20 + f * 180);
        px[i + 1] = Math.round(45 + f * 130);
        px[i + 2] = Math.round(110 + f * 20);
        px[i + 3] = 255;
    }
}
const skyTex = efx.graphics.createTexture(
    efx.graphics.createImageData(W, H, px), { wrap: 'clamp' });

// inward-facing cube: the inside is front-facing under BACK/CCW culling
const sky = efx.graphics.createMesh(
    efx.graphics.makeCube({ size: 20, inverted: true }));
sky.setSurfaceMaterial(0, {
    unlit: true,
    diffuse: { color: [1, 1, 1, 1], map: skyTex },
});

// a lit cube beyond the sky's far face
const far = efx.graphics.createMesh(efx.graphics.makeCube({ size: 2 }));
far.setSurfaceMaterial(0, {
    diffuse: { color: [0.9, 0.5, 0.2, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
});

// a lit cube in front of the sky
const near = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.5 }));
near.setSurfaceMaterial(0, { diffuse: { color: [0.2, 0.7, 0.9, 1] } });

// light from the camera side so the cube faces toward the viewer are lit
efx.graphics.setDirectionalLight({ dir: [0.2, -0.5, -0.9], color: [1, 1, 1, 1] });

function update() {}

function render() {
    efx.graphics.drawMesh(sky, {
        transform: efx.math.mat4.identity(),
        depthWrite: false,
    });
    efx.graphics.drawMesh(far, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [4, 0, -12]),
    });
    efx.graphics.drawMesh(near, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), [-1.3, 0, -3]),
    });
}
