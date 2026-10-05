// Blend golden: blend is frame-local render state. The mode set once at load
// must not leak into a frame (the engine resets it to alpha), and per-draw /
// per-batch overrides select a mode for one draw only. Each overlay sits on a
// grey base so alpha (0.4), additive (0.75) and subtractive (0.0) differ.
efx.graphics.setClearColor([0, 0, 0, 1]);
const tex = efx.graphics.createTexture(
    efx.graphics.createImageData(2, 2, [200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255]));

// set once at load: must NOT carry into the frame
efx.graphics.setBlendMode('additive');

function update() {}

function render() {
    // grey bases (no override => the frame's alpha state)
    efx.graphics.drawQuad(tex, 60, 80, { size: [180, 180], color: [0.35, 0.35, 0.35, 1] });
    efx.graphics.drawQuad(tex, 300, 80, { size: [180, 180], color: [0.35, 0.35, 0.35, 1] });
    efx.graphics.drawQuad(tex, 540, 80, { size: [180, 180], color: [0.35, 0.35, 0.35, 1] });

    // 1) frame reset: no override -> alpha, not the load-time additive
    efx.graphics.drawQuad(tex, 60, 80, { size: [180, 180], color: [0.4, 0.4, 0.4, 1] });
    // 2) per-draw additive override
    efx.graphics.drawQuad(tex, 300, 80, { size: [180, 180], color: [0.4, 0.4, 0.4, 1], blend: 'additive' });
    // 3) per-draw subtractive override
    efx.graphics.drawQuad(tex, 540, 80, { size: [180, 180], color: [0.4, 0.4, 0.4, 1], blend: 'subtractive' });

    // 4) batch-level sprite blend applies to every sprite in the call
    efx.graphics.drawSprites(tex, [
        { x: 60, y: 300, size: [180, 180], color: [0.35, 0.35, 0.35, 1] },
        { x: 60, y: 300, size: [180, 180], color: [0.4, 0.4, 0.4, 1] },
    ], { blend: 'additive' });
}
