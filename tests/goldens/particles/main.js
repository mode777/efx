// F11 golden scene — billboards, world-space particles (view), and a fixed
// oriented `plane` particle sheet. Particles are emitted as a static burst
// (speed 0, gravity 0, constant size/color, no spin) so the frame is
// independent of the frame time and therefore stable as a golden.

function makeGlow(size) {
    const px = new Uint8Array(size * size * 4);
    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const dx = ((x + 0.5) / size) * 2 - 1;
            const dy = ((y + 0.5) / size) * 2 - 1;
            const d = Math.min(1, Math.sqrt(dx * dx + dy * dy));
            const a = Math.round((1 - d) * (1 - d) * 255);
            const i = (y * size + x) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = a;
        }
    }
    return px;
}

const glow = efx.createTexture(
    efx.createImageData({ width: 16, height: 16, pixels: makeGlow(16) }),
);

efx.setClearColor([0.02, 0.02, 0.06, 1]);
efx.setCamera3D({ pos: [0, 1.5, 8], target: [0, 0.5, 0], fov: 60 });

const sparks = efx.createParticleSystem({
    texture: glow,
    max: 128,
    lifetime: 1000,
    emissionRate: 0,
    position: [0, 0.7, 0],
    speed: 0,
    sizes: [0.5],
    colors: [1, 0.7, 0.25, 1],
    blend: 'additive',
    facing: 'view',
    emissionShape: { shape: 'box', size: [1.2, 0.2, 1.2] },
});
sparks.emit(64);

const water = efx.createParticleSystem({
    texture: glow,
    max: 64,
    lifetime: 1000,
    emissionRate: 0,
    position: [0, 0, 0],
    speed: 0,
    sizes: [0.8],
    colors: [0.2, 0.5, 1, 0.6],
    blend: 'alpha',
    facing: 'plane',
    normal: [0, 1, 0],
    emissionShape: { shape: 'box', size: [1.5, 0, 1.5] },
});
water.emit(16);

efx.registerRenderHook(() => {
    efx.drawParticles(water);
    efx.drawBillboard([-1.4, 1.1, 0], {
        texture: glow,
        size: 0.7,
        color: [1, 0.3, 0.2, 1],
    });
    efx.drawBillboard([1.4, 0.6, 0], {
        texture: glow,
        size: [0.4, 1.0],
        facing: 'y',
        color: [0.4, 1, 0.5, 1],
    });
    efx.drawParticles(sparks);
});
