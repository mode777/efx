// F11 particle/billboard API: portable semantics (config validation,
// lifecycle, and error types) identical on the desktop runtime and the web
// bridge. Actual rendering is covered by the particles golden scene.
function kind(fn) {
    try {
        fn();
        return 'none';
    } catch (e) {
        if (e instanceof TypeError) return 'TypeError';
        if (e instanceof RangeError) return 'RangeError';
        return 'Error';
    }
}

const img = efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) });
const tex = efx.createTexture(img);

const ps = efx.createParticleSystem({
    texture: tex,
    max: 64,
    lifetime: [1, 2],
    emissionRate: 10,
    position: [0, 0, 0],
    direction: [0, 1, 0],
    speed: [1, 2],
    gravity: [0, -1, 0],
    sizes: [1, 3],
    colors: [[1, 0, 0, 1], [1, 1, 0, 0]],
    facing: 'view',
    blend: 'additive',
    emissionShape: { shape: 'sphere', size: [1, 1, 1] },
});
if (ps.count !== 0) throw new Error('fresh count ' + ps.count);
ps.emit(5);
if (ps.count !== 5) throw new Error('emit count ' + ps.count);
ps.speedScale = 2;
if (ps.speedScale !== 2) throw new Error('speedScale');
ps.set({ emissionRate: 0, position: [1, 0, 0] });
ps.pause();
ps.start();
ps.reset();
if (ps.count !== 0) throw new Error('reset count ' + ps.count);

// validation matrix (missing/typed/ranged)
if (kind(() => efx.createParticleSystem({ max: 4, lifetime: 1 })) !== 'TypeError') throw new Error('missing texture');
if (kind(() => efx.createParticleSystem({ texture: tex, lifetime: 1 })) !== 'TypeError') throw new Error('missing max');
if (kind(() => efx.createParticleSystem({ texture: tex, max: 0, lifetime: 1 })) !== 'RangeError') throw new Error('max range');
if (kind(() => efx.createParticleSystem({ texture: tex, max: 4 })) !== 'TypeError') throw new Error('missing lifetime');
if (kind(() => efx.createParticleSystem({ texture: tex, max: 4, lifetime: [2, 1] })) !== 'RangeError') throw new Error('bad lifetime order');
if (kind(() => efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1, bogus: 1 })) !== 'TypeError') throw new Error('unknown field');
if (kind(() => efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1, facing: 'sideways' })) !== 'TypeError') throw new Error('bad facing');
if (kind(() => efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1, space: 'screen', facing: 'plane' })) !== 'TypeError') throw new Error('plane+screen');
if (kind(() => {
    const p = efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1 });
    p.set({ max: 0 });
}) !== 'RangeError') throw new Error('bad set');
if (kind(() => {
    const p = efx.createParticleSystem({ texture: tex, max: 4, lifetime: 1 });
    p.destroy();
    p.emit(1);
}) !== 'TypeError') throw new Error('destroyed use');

// billboards/sprites validate before recording (no surface needed to throw)
if (kind(() => efx.drawBillboard([0, 0, 0], { size: [1, 1] })) !== 'TypeError') throw new Error('billboard texture');
if (kind(() => efx.drawBillboard([0, 0], { texture: tex })) !== 'TypeError') throw new Error('billboard pos');
if (kind(() => efx.drawBillboard([0, 0, 0], { texture: tex, size: [0, 1] })) !== 'RangeError') throw new Error('billboard size');
if (kind(() => efx.drawSprites(tex, 'nope')) !== 'TypeError') throw new Error('sprites array');
if (kind(() => efx.drawSprites(tex, [{ size: [1, 1] }])) !== 'TypeError') throw new Error('sprite x/y');

ps.destroy();
tex.destroy();

efx.log('s-11-particles-ok');
