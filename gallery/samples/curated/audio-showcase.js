// Audio Showcase — an interactive F14 tour. One streamed background track plus
// the engine's 32-voice effect bank: click a pad (or press 1/2/3) to fire a
// blip/chime/thud with pan from the click x and pitch from the click y, Space
// pauses/resumes the music, M mutes, and [ / ] change volume. On the web the
// audio context unlocks on your first click or key press (efx.audio.resume is
// called for you). Everything is mixed by the engine — no channels here.

efx.setClearColor([0.03, 0.035, 0.06, 1]);
efx.setCamera2D({ frame: [640, 480] });

const FW = 640;
const FH = 480;
const white = efx.whiteTexture;

// ---- assets from the sample's pack (mounted as the resource root) ----
const fd = efx.loadFontData('font.ttf');
const fontTitle = efx.createFont(fd, { size: 24 });
const font = efx.createFont(fd, { size: 15 });
const fontSmall = efx.createFont(fd, { size: 12 });

// ---- audio ----
// playBackgroundMusic streams the track; on the web it begins after the first
// interaction unlocks the audio context.
const music = efx.audio.playBackgroundMusic('music.mp3', { loop: true, volume: 0.5 });
let vol = 0.5;
let muted = false;

const PADS = [
    { label: '1  Blip', path: 'blip.wav', color: [0.30, 0.80, 1.00] },
    { label: '2  Chime', path: 'chime.wav', color: [0.65, 0.55, 1.00] },
    { label: '3  Thud', path: 'thud.wav', color: [1.00, 0.55, 0.35] },
];
const PAD_W = 170;
const PAD_H = 90;
const PAD_GAP = 30;
const PAD_Y = 250;
const PAD_X0 = (FW - (PAD_W * 3 + PAD_GAP * 2)) / 2;

let winW = FW;
let winH = FH;
function readWindow() {
    const s = efx.window.size;
    if (s[0] > 0) {
        winW = s[0];
    }
    if (s[1] > 0) {
        winH = s[1];
    }
}
function toFrame(x, y) {
    return [x * (FW / winW), y * (FH / winH)];
}

let unlocked = false;
let pulses = [0, 0, 0];
let t = 0;

function unlock() {
    if (!unlocked) {
        unlocked = true;
        try {
            efx.audio.resume();
        } catch (e) {
            // no audio context yet; the next interaction retries
        }
    }
}

function setVolume(v) {
    vol = Math.max(0, Math.min(1, v));
    music.setVolume(muted ? 0 : vol);
}

function playPad(i, pan, pitch) {
    unlock();
    // No device / all voices busy => null; the demo simply shows no pulse.
    const s = efx.audio.playAudioEffect(PADS[i].path, {
        volume: 0.9,
        pan: pan,
        pitch: pitch,
    });
    if (s) {
        pulses[i] = 1.0;
    }
}

efx.mouse.onDown(function (e) {
    unlock();
    const f = toFrame(e.x, e.y);
    for (let i = 0; i < 3; i++) {
        const x = PAD_X0 + i * (PAD_W + PAD_GAP);
        if (f[0] >= x && f[0] <= x + PAD_W && f[1] >= PAD_Y && f[1] <= PAD_Y + PAD_H) {
            const pan = ((f[0] - x) / PAD_W) * 2 - 1;
            const pitch = 0.6 + (1 - (f[1] - PAD_Y) / PAD_H);
            playPad(i, pan, pitch);
            return;
        }
    }
    playPad(1, (f[0] / FW) * 2 - 1, 1.0);
});

function update(dt) {
    readWindow();
    t += dt;
    for (let i = 0; i < 3; i++) {
        pulses[i] = Math.max(0, pulses[i] - dt * 2.2);
    }
    if (efx.keyboard.isPressed('1')) {
        playPad(0, 0, 1.0);
    }
    if (efx.keyboard.isPressed('2')) {
        playPad(1, 0, 1.0);
    }
    if (efx.keyboard.isPressed('3')) {
        playPad(2, 0, 1.0);
    }
    if (efx.keyboard.isPressed('space')) {
        unlock();
        if (music.playing) {
            music.pause();
        } else {
            music.resume();
        }
    }
    if (efx.keyboard.isPressed('m')) {
        muted = !muted;
        setVolume(vol);
    }
    if (efx.keyboard.isPressed('leftbracket')) {
        setVolume(vol - 0.1);
    }
    if (efx.keyboard.isPressed('rightbracket')) {
        setVolume(vol + 0.1);
    }
}

function render() {
    efx.drawText('Audio Showcase', fontTitle, 24, 20, { color: [1, 1, 1, 1] });
    efx.drawText('F14: one streamed track + a 32-voice effect bank, mixed by the engine.',
                 fontSmall, 24, 52, { color: [0.7, 0.75, 0.85, 1] });

    const playing = music.playing;
    let status;
    if (playing) {
        status = 'playing';
    } else if (!unlocked) {
        status = 'click to unlock';
    } else {
        status = 'paused';
    }
    efx.drawText('Music  ' + status + '   vol ' + Math.round(vol * 100) + '%' +
                 (muted ? '  (muted)' : ''),
                 font, 24, 92,
                 { color: playing ? [0.55, 1, 0.7, 1] : [1, 0.8, 0.5, 1] });

    // A visual-only level meter: it animates while the stream is playing.
    const meterW = 320;
    efx.drawQuad(24, 114, white, { size: [meterW, 10], color: [0.14, 0.17, 0.24, 1] });
    if (playing) {
        const lv = 0.5 + 0.5 * Math.sin(t * 6.0) * Math.sin(t * 2.3);
        efx.drawQuad(24, 114, white, {
            size: [meterW * Math.max(0.08, lv), 10],
            color: [0.35, 0.85, 1, 0.9],
        });
    }

    for (let i = 0; i < 3; i++) {
        const x = PAD_X0 + i * (PAD_W + PAD_GAP);
        const c = PADS[i].color;
        const pulse = pulses[i];
        efx.drawQuad(x, PAD_Y, white, {
            size: [PAD_W, PAD_H],
            color: [c[0] * 0.22, c[1] * 0.22, c[2] * 0.22, 1],
        });
        const b = 2 + pulse * 7;
        efx.drawQuad(x - b, PAD_Y - b, white, {
            size: [PAD_W + 2 * b, PAD_H + 2 * b],
            color: [c[0], c[1], c[2], 0.2 + 0.6 * pulse],
        });
        efx.drawText(PADS[i].label, font, x + 14, PAD_Y + 16,
                     { color: [1, 1, 1, 0.95] });
        efx.drawText('pan = x, pitch = y', fontSmall, x + 14, PAD_Y + PAD_H - 26,
                     { color: [0.8, 0.85, 0.95, 0.8] });
    }

    efx.drawText('Click a pad (or 1/2/3) to fire an effect. Space = pause/resume music.',
                 fontSmall, 24, 386, { color: [0.75, 0.8, 0.9, 1] });
    efx.drawText('M = mute.  [ / ] = volume.  On the web, audio unlocks on first input.',
                 fontSmall, 24, 406, { color: [0.55, 0.6, 0.7, 1] });

    const a = 0.35 + 0.3 * Math.sin(t * 3.0);
    efx.drawQuad(24, 438, white, { size: [FW - 48, 2], color: [0.3, 0.6, 1, a] });
}

efx.registerUpdateHook(update);
efx.registerRenderHook(render);
