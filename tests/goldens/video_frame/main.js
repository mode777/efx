// video-playback golden: the committed MPEG-1 fixture is opened through the
// engine's video path, played, and drawn full-frame. The capture pins a frame
// after a couple of engine ticks, so the committed golden proves the whole
// pipeline end-to-end: MPEG-1 decode -> dynamic-texture upload -> sampling by
// an ordinary draw. Each tick decodes exactly one source frame at 25 fps, so
// the captured image is a specific frame of the clip on every target.
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const vid = efx.video.openVideo('clip.mpg');
vid.play();

function render() {
    efx.graphics.drawQuad(vid.texture, 128, 48, { size: [384, 384] });
}
