// A declarative post-effect chain: bloom plus a color filter.
efx.graphics.setClearColor([0.02, 0.02, 0.05, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const tex = efx.graphics.createTexture(efx.graphics.createImageData(2, 2, [
        230, 40, 40, 255,   40, 210, 80, 255,
        40, 80, 230, 255,   240, 220, 60, 255,
    ]));

efx.graphics.setPostEffects([
    { effect: 'bloom', threshold: 0.6, strength: 0.9 },
    { effect: 'colorFilter', saturation: 1.1, contrast: 1.15 },
]);

let t = 0;
function update(dt) { t += dt; }
function render() {
    efx.graphics.drawQuad(tex, 0, 0, { size: [640, 480] });
    efx.graphics.drawQuad(efx.graphics.whiteTexture, 120, 110, { size: [200, 80], color: efx.color.white });
    efx.graphics.drawQuad(tex, 360, 180, {
        size: [160, 160],
        rotation: t * 57.3,
        color: [0.9, 0.35, 0.2, 1],
    });
}
