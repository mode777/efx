// Audio Showcase — an interactive F14 tour of the handle-based audio API. One
// streamed background track (faded in through a plain handle write) plus
// fully-decoded effects fired with pan/pitch from the click position. Space
// pauses/resumes the track, M mutes, and [ / ] change the target volume. On the
// web the audio context unlocks on your first click or key press
// (efx.audio.resume is called for you). Everything is mixed by the engine — no
// channels here.

efx.graphics.setClearColor([0.03, 0.035, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const FW = 640;
const FH = 480;
// The white texture needs a GPU context, so fetch it lazily at render time
// (the sample also runs headless in --script mode, where there is none).
let whiteTex = null;
function white() {
    if (!whiteTex) {
        whiteTex = efx.whiteTexture;
    }
    return whiteTex;
}

// ---- assets from the sample's pack (mounted as the resource root) ----
const fd = efx.graphics.loadFontData('font.ttf');
const fontTitle = efx.graphics.createFont(fd, { size: 24 });
const font = efx.graphics.createFont(fd, { size: 15 });
const fontSmall = efx.graphics.createFont(fd, { size: 12 });

// ---- audio ----
// A streamed source is decoded incrementally by each playhead; playAudio
// returns the handle we control (volume/pitch/pan/loop + pause/stop).
const musicStream = efx.audio.loadAudioStream('music.mp3');
const music = efx.audio.playAudio(musicStream, { loop: true, volume: 0.0 });
let targetVol = 0.5; // fade the handle toward this
let muted = false;

const PADS = [
    { label: '1  Blip', path: 'blip.wav', color: [0.30, 0.80, 1.00] },
    { label: '2  Chime', path: 'chime.wav', color: [0.65, 0.55, 1.00] },
    { label: '3  Thud', path: 'thud.wav', color: [1.00, 0.55, 0.35] },
];
// Fully-decoded sources; one buffer can back many overlapping playheads.
const PAD_DATA = PADS.map(p => efx.audio.loadAudioData(p.path));
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

function playPad(i, pan, pitch) {
    unlock();
    // No device / all voices busy => null; the demo simply shows no pulse.
    const s = efx.audio.playAudio(PAD_DATA[i], {
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
    // A fade is just writing the handle's volume every frame.
    const want = muted ? 0 : targetVol;
    const cur = music.volume;
    const next = cur + Math.sign(want - cur) * Math.min(Math.abs(want - cur), dt * 2.0);
    music.volume = next;

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
    }
    if (efx.keyboard.isPressed('leftbracket')) {
        targetVol = Math.max(0, targetVol - 0.1);
    }
    if (efx.keyboard.isPressed('rightbracket')) {
        targetVol = Math.min(1, targetVol + 0.1);
    }
}

function render() {
    efx.graphics.drawText('Audio Showcase', fontTitle, 24, 20, { color: [1, 1, 1, 1] });
    efx.graphics.drawText('F14: one streamed track + decoded effects, controlled through handles.',
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
    efx.graphics.drawText('Music  ' + status + '   vol ' + Math.round(music.volume * 100) + '%' +
                 (muted ? '  (muted)' : ''),
                 font, 24, 92,
                 { color: playing ? [0.55, 1, 0.7, 1] : [1, 0.8, 0.5, 1] });

    // A visual-only level meter: it animates while the stream is playing.
    const meterW = 320;
    efx.graphics.drawQuad(24, 114, white(), { size: [meterW, 10], color: [0.14, 0.17, 0.24, 1] });
    const lvl = music.volume;
    efx.graphics.drawQuad(24, 114, white(), {
        size: [meterW * Math.max(0.02, lvl), 10],
        color: [0.35, 0.85, 1, 0.9],
    });

    for (let i = 0; i < 3; i++) {
        const x = PAD_X0 + i * (PAD_W + PAD_GAP);
        const c = PADS[i].color;
        const pulse = pulses[i];
        efx.graphics.drawQuad(x, PAD_Y, white(), {
            size: [PAD_W, PAD_H],
            color: [c[0] * 0.22, c[1] * 0.22, c[2] * 0.22, 1],
        });
        const b = 2 + pulse * 7;
        efx.graphics.drawQuad(x - b, PAD_Y - b, white(), {
            size: [PAD_W + 2 * b, PAD_H + 2 * b],
            color: [c[0], c[1], c[2], 0.2 + 0.6 * pulse],
        });
        efx.graphics.drawText(PADS[i].label, font, x + 14, PAD_Y + 16,
                     { color: [1, 1, 1, 0.95] });
        efx.graphics.drawText('pan = x, pitch = y', fontSmall, x + 14, PAD_Y + PAD_H - 26,
                     { color: [0.8, 0.85, 0.95, 0.8] });
    }

    efx.graphics.drawText('Click a pad (or 1/2/3) to fire an effect. Space = pause/resume music.',
                 fontSmall, 24, 386, { color: [0.75, 0.8, 0.9, 1] });
    efx.graphics.drawText('M = mute.  [ / ] = volume (the handle fades toward it).',
                 fontSmall, 24, 406, { color: [0.55, 0.6, 0.7, 1] });

    const a = 0.35 + 0.3 * Math.sin(t * 3.0);
    efx.graphics.drawQuad(24, 438, white(), { size: [FW - 48, 2], color: [0.3, 0.6, 1, a] });
}

efx.registerUpdateHook(update);
efx.registerRenderHook(render);
