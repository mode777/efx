// F11 oriented-plane showcase: a water surface built from fixed `plane`
// particles (normal +Y). The camera orbits, but the quads stay world-aligned
// because they are not billboards.

// the water sprite is the shared radial image (white, alpha falloff)
const tex = efx.graphics.createTexture(efx.graphics.loadImage('radial.png'));

efx.graphics.setClearColor([0.01, 0.03, 0.06, 1]);
efx.graphics.setCamera3D([0, 3, 8], [0, 0, 0], 60);

const water = efx.graphics.createParticleSystem(tex, 400, 1000, { emissionRate: 0, position: [0, 0, 0], speed: 0, sizes: [0.9], colors: [0.2, 0.5, 1, 0.45], sizeVariation: 0.6, blend: 'alpha', facing: 'plane', normal: [0, 1, 0], emissionShape: { shape: 'box', size: [4, 0, 4] } });
water.emit(220);

const spray = efx.graphics.createParticleSystem(tex, 120, [0.6, 1.4], { emissionRate: 30, position: [0, 0.05, 0], direction: [0, 1, 0], spread: 40, speed: [0.5, 1.4], gravity: [0, -2.5, 0], sizes: [0.25, 0.05], colors: [[0.7, 0.9, 1, 0.9], [0.3, 0.6, 1, 0]], blend: 'additive', facing: 'view' });

let t = 0;
efx.registerUpdateHook((dt) => {
    t = efx.time.now;
    const a = t * 0.5;
    efx.graphics.setCamera3D([Math.cos(a) * 8, 2.8, Math.sin(a) * 8], [0, 0.2, 0], 60);
});

efx.registerRenderHook(() => {
    efx.graphics.drawParticles(water); // fixed oriented planes
    efx.graphics.drawParticles(spray); // camera-facing spray above the surface
    efx.graphics.drawBillboard(tex, [0, 1.1, 0], { size: 0.5, color: [0.6, 0.9, 1, 0.9], facing: 'y' });
});
