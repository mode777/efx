// video-playback golden: the committed MPEG-1 fixture is opened through the
// engine's video path, played, and drawn. The capture pins a frame after a
// couple of engine ticks, so the committed golden proves the whole pipeline
// end-to-end: MPEG-1 decode -> dynamic-texture upload -> sampling by an
// ordinary draw.
//
// The fixture is a spatially-uniform clip (one solid colour per frame, hue
// rotating): video textures sample with linear filtering, and a half-texel
// sampling convention difference between renderers (llvmpipe / WARP / Metal)
// shifts every pixel by the local gradient. A solid frame has zero spatial
// gradient, so the decoded colour — and the whole decode -> upload -> draw
// pipeline — is identical on every renderer; the core unit suite covers
// spatial decode fidelity and the playback state machine.
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const vid = efx.video.openVideo('clip.mpg');
vid.play();

function update() {}

function render() {
    efx.graphics.drawQuad(vid.texture, 128, 48, { size: [384, 384] });
}
