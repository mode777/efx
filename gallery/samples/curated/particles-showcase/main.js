// F11 particle showcase: an additive fire, an alpha smoke plume, and a
// y-axis-billboard ember burst (burst + continuous emission), plus a 2D
// drawSprites row. All public API; the sprite is the shared radial image.

const spark = efx.graphics.createTexture(efx.graphics.loadImage('radial.png'));

efx.graphics.setClearColor([0.02, 0.02, 0.05, 1]);
efx.graphics.setCamera3D([0, 2.2, 7], [0, 1.2, 0], 60);

const fire = efx.graphics.createParticleSystem(spark, 600, [0.4, 0.9], { emissionRate: 140, position: [0, 0.1, 0], direction: [0, 1, 0], spread: 22, speed: [0.8, 1.8], gravity: [0, 0.6, 0], sizes: [0.55, 0.05], colors: [[1, 0.9, 0.45, 0.95], [1, 0.25, 0.05, 0]], blend: 'additive', facing: 'view' });

const smoke = efx.graphics.createParticleSystem(spark, 300, [1.5, 3.0], { emissionRate: 24, position: [0, 0.3, 0], direction: [0, 1, 0], spread: 38, speed: [0.4, 1.0], gravity: [0, 0.25, 0], sizes: [0.5, 1.8], colors: [[0.45, 0.45, 0.5, 0.3], [0.25, 0.25, 0.3, 0]], blend: 'alpha', facing: 'view' });

const embers = efx.graphics.createParticleSystem(spark, 200, [1.2, 2.2], { emissionRate: 0, position: [0, 0.5, 0], direction: [0, 1, 0], spread: 60, speed: [1.5, 3.0], gravity: [0, -1.5, 0], sizes: [0.18], colors: [1, 0.7, 0.3, 1], blend: 'additive', facing: 'y' });

efx.graphics.setCamera2D({ frame: [640, 480] });

let t = 0;
let burst = 0;
efx.registerUpdateHook((dt) => {
    t = efx.time.now;
    const a = t * 0.4;
    efx.graphics.setCamera3D([Math.cos(a) * 7, 2.2, Math.sin(a) * 7], [0, 1.2, 0], 60);
    burst -= dt;
    if (burst <= 0) {
        burst = 2.2;
        embers.emit(40); // burst alongside the continuous fire/smoke
    }
});

efx.registerRenderHook(() => {
    efx.graphics.drawParticles(smoke);
    efx.graphics.drawParticles(fire);
    efx.graphics.drawBillboard(spark, [0, 0.4, 0], { size: 0.9, color: [1, 0.7, 0.3, 0.9], blend: 'additive' });
    efx.graphics.drawParticles(embers);
    // a 2D sprite row (drawSprites is 2D-only)
    efx.graphics.drawSprites(spark, [
        { x: 20, y: 20, size: [48, 48], color: [1, 0.4, 0.2, 0.9] },
        { x: 74, y: 20, size: [48, 48], color: [1, 0.7, 0.3, 0.9] },
        { x: 128, y: 20, size: [48, 48], color: [0.5, 0.8, 1, 0.9] },
    ], { blend: 'additive' });
});
