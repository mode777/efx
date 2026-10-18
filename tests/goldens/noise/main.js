// Noise golden (ADR 0059): every pixel is a deterministic function of the
// noise engine — four noise-generated textures blitted through
// fill2/imageData -> createTexture -> drawQuad. The scene pins the fractal
// path (simplex fBm, 4 octaves), cellular's centered distance range, the
// domain-warp path, the alpha-mode byte placement (white RGB, noise in the
// alpha channel — same seed as the first quad, so its visible pattern must
// mirror it), and three independent seeds. Any cross-target float drift in
// the noise core breaks the image (the ADR 0020 tolerance absorbs
// rasterizer jitter, not noise disagreement).
efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const texFbm = efx.graphics.createTexture(efx.noise.imageData(128, 128, {
    seed: 7, type: 'opensimplex2', frequency: 0.02, octaves: 4,
    domain: [-1, 1],
}));
const texCell = efx.graphics.createTexture(efx.noise.imageData(128, 128, {
    seed: 9, type: 'cellular', frequency: 0.08,
    domain: [-1, 0.15],
}));
const texWarp = efx.graphics.createTexture(efx.noise.imageData(128, 128, {
    seed: 13, type: 'value', frequency: 0.02, octaves: 3,
    warp: 'opensimplex2', domain: [-1, 1],
}));
const texAlpha = efx.graphics.createTexture(efx.noise.imageData(128, 128, {
    seed: 7, type: 'opensimplex2', frequency: 0.02, octaves: 4,
    domain: [-1, 1], alpha: true,
}));

function update() {}

function render() {
    efx.graphics.drawQuad(texFbm, 10, 96, { size: [140, 288] });
    efx.graphics.drawQuad(texCell, 170, 96, { size: [140, 288] });
    efx.graphics.drawQuad(texWarp, 330, 96, { size: [140, 288] });
    efx.graphics.drawQuad(texAlpha, 490, 96, { size: [140, 288] });
}
