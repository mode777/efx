// Video Showcase — an interactive tour of the MPEG-1 video API. The engine
// decodes the clip and uploads each frame to `video.texture`, an ordinary
// dynamic texture you draw with drawQuad. Video audio is engine-managed: it
// plays through the mixer while the video plays and mutes with `vid.muted`.
// Space pauses/resumes, M mutes, L toggles looping.
//
// The clip is a short excerpt of "Big Buck Bunny" (Blender Foundation,
// CC-BY 3.0) re-encoded to MPEG-1; see CREDITS.md.

efx.graphics.setClearColor([0.03, 0.035, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const fd = efx.graphics.loadFontData('font.ttf');
const fontTitle = efx.graphics.createFont(fd, 20);
const font = efx.graphics.createFont(fd, 13);

const vid = efx.video.openVideo('clip.mpg', { loop: true });
vid.play();

// fit the video into the frame, preserving aspect
const scale = Math.min(600 / vid.width, 400 / vid.height);
const vw = vid.width * scale;
const vh = vid.height * scale;
const vx = (640 - vw) / 2;
const vy = 40;

let t = 0;

function update(dt) {
    t = efx.time.now;
    if (efx.keyboard.isPressed('space')) {
        if (vid.currentTime > 0 && !vid.ended) {
            vid.pause();
        } else {
            vid.play();
        }
    }
    if (efx.keyboard.isPressed('m')) {
        vid.muted = !vid.muted;
    }
    if (efx.keyboard.isPressed('l')) {
        vid.loop = !vid.loop;
    }
}

function render() {
    // the video frame is just a texture
    efx.graphics.drawQuad(vid.texture, vx, vy, { size: [vw, vh] });

    efx.graphics.drawText('Video Showcase', fontTitle, 20, 12,
                          { color: efx.color.white });
    efx.graphics.drawText('clip: "Big Buck Bunny" (c) Blender Foundation, CC-BY 3.0',
                          font, 20, 444, { color: [0.45, 0.5, 0.6, 1] });
    const state = vid.ended ? 'ended' : (vid.currentTime > 0 ? 'playing' : 'paused');
    efx.graphics.drawText(
        'MPEG-1  ' + vid.width + 'x' + vid.height +
        '  ' + vid.currentTime.toFixed(1) + 's / ' + vid.duration.toFixed(1) + 's' +
        '  [' + state + ']', font, 20, 458,
        { color: [0.7, 0.75, 0.85, 1] });
    efx.graphics.drawText(
        'space play/pause   m ' + (vid.muted ? 'unmute' : 'mute') +
        '   l loop:' + (vid.loop ? 'on' : 'off'), font, 20, 470,
        { color: [0.55, 0.6, 0.7, 1] });
}
