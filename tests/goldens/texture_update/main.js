// dynamic-textures golden: a `{ dynamic: true }` texture is replaced once per
// frame via `update()` with a frame-index-driven pattern, then drawn
// full-frame. The capture pins a single frame, so the committed golden proves
// both that updates reach the sampled texture and exactly which update is on
// screen (each frame's pattern differs).
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const S = 64;
const tex = efx.graphics.createTexture(
    efx.graphics.createImageData(S, S, new Uint8Array(S * S * 4)),
    { dynamic: true });

let frame = 0;
function update() {
    frame++;
    const px = new Uint8Array(S * S * 4);
    for (let y = 0; y < S; y++) {
        for (let x = 0; x < S; x++) {
            const i = (y * S + x) * 4;
            const band = ((x + y) / 8) | 0;
            px[i + 0] = (band * 36 + frame * 40) % 256;
            px[i + 1] = (y * 4 + 256 - frame * 24) % 256;
            px[i + 2] = (x * 4 + frame * 16) % 256;
            px[i + 3] = 255;
        }
    }
    tex.update(efx.graphics.createImageData(S, S, px));
}

function render() {
    efx.graphics.drawQuad(tex, 32, 32, { size: [576, 416] });
}
