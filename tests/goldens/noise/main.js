// Noise golden (ADR 0059): every pixel is a deterministic function of the
// noise engine — three noise-generated textures blitted through
// fill2/imageData -> createTexture -> drawQuad. The scene pins the fractal
// path (simplex fBm, 4 octaves), cellular's centered distance range, the
// domain-warp path, and three independent seeds. Any cross-target float
// drift in the noise core breaks the image (the ADR 0020 tolerance absorbs
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

function update() {}

function render() {
    efx.graphics.drawQuad(texFbm, 32, 96, { size: [176, 288] });
    efx.graphics.drawQuad(texCell, 232, 96, { size: [176, 288] });
    efx.graphics.drawQuad(texWarp, 432, 96, { size: [176, 288] });
}
