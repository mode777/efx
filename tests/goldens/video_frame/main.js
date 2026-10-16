// video-playback golden: the committed MPEG-1 fixture is opened through the
// engine's video path, played, and drawn. The capture pins a frame after a
// couple of engine ticks, so the committed golden proves the whole pipeline
// end-to-end: MPEG-1 decode -> dynamic-texture upload -> sampling by an
// ordinary draw.
//
// The frame is drawn at its native 64x64 size (tiled), so every draw is a
// 1:1 texel-to-pixel fetch: sampling is exact and therefore identical on
// every renderer. (A magnified draw would reintroduce linear-filtering
// precision differences between llvmpipe and WARP on testsrc2's 1px bars.)
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const vid = efx.video.openVideo('clip.mpg');
vid.play();

function update() {}

function render() {
    const s = vid.width; // 64
    const cols = 6, rows = 4;
    const x0 = (640 - cols * s) / 2;
    const y0 = (480 - rows * s) / 2;
    for (let ty = 0; ty < rows; ty++) {
        for (let tx = 0; tx < cols; tx++) {
            efx.graphics.drawQuad(vid.texture, x0 + tx * s, y0 + ty * s,
                                  { size: [s, s] });
        }
    }
}
