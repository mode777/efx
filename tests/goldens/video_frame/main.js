// video-playback golden: the committed MPEG-1 fixture is opened through the
// engine's video path, played, and drawn. The capture pins a frame after a
// couple of engine ticks, so the committed golden proves the whole pipeline
// end-to-end: MPEG-1 decode -> dynamic-texture upload -> sampling by an
// ordinary draw.
//
// The fixture is a constant-colour clip (every frame the same solid colour).
// Two cross-renderer hazards make textured video goldens fragile, and a
// constant frame sidesteps both: (1) video textures sample with linear
// filtering, so a half-texel sampling convention difference between renderers
// shifts every pixel by the local gradient — a solid frame has zero gradient;
// (2) the engine advances the playhead by the display frame duration, which
// differs per runner, so the decoded frame index is not portable — a constant
// clip makes every frame identical. The golden therefore proves decode ->
// dynamic-texture upload -> sampling by an ordinary draw on every target; the
// core unit suite and the api tests cover spatial decode fidelity, playback
// advancement, looping, and the end/restart state machine.
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const vid = efx.video.openVideo('clip.mpg');
vid.play();

function update() {}

function render() {
    efx.graphics.drawQuad(vid.texture, 128, 48, { size: [384, 384] });
}
