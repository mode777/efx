// Glow Gauntlet — rung 3 of the game gallery series. The first rung where
// gamepad and streamed music carry the experience: a one-button "copter"
// dodger. Hold the single control to rise, release to sink, and thread
// scrolling gates. Keyboard, mouse, and gamepad face button map identically.
// It self-plays until the first input.

efx.graphics.setClearColor([0.02, 0.015, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const FRAME_W = 640;
const FRAME_H = 480;

const AVATAR_X = 150;
const AVATAR_W = 28;
const AVATAR_H = 22;

const GRAVITY = 1050;
const THRUST = 2050;
const VY_MAX = 430;

const GATE_W = 26;
const GATE_GAP_MAX = 210;
const GATE_GAP_MIN = 118;
const GATE_SPACING = 236;

const SPEED_BASE = 175;
const SPEED_PER_POINT = 4.2;
const SPEED_MAX = 390;

const COL_AVATAR = [0.4, 1.0, 0.95, 1];
const COL_GATE = [0.85, 0.25, 0.95, 1];
const COL_TEXT = [0.85, 0.9, 1.0, 1];

// ---- fonts ---------------------------------------------------------------
const hudFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 22, {
    outline: { width: 1 },
    shadow: { blur: 3, offset: [1, 1] },
});
const bigFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 44, {
    outline: { width: 2 },
});

// ---- procedural textures -------------------------------------------------
function radial(size, cr, cg, cb) {
    const px = [];
    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const dx = (x + 0.5) / size * 2 - 1;
            const dy = (y + 0.5) / size * 2 - 1;
            const d = Math.min(1, Math.sqrt(dx * dx + dy * dy));
            const a = Math.round((1 - d) * (1 - d) * 255);
            px.push(cr, cg, cb, a);
        }
    }
    return efx.graphics.createTexture(efx.graphics.createImageData(size, size, px));
}
const glowTex = radial(48, 255, 255, 255);

const burst = efx.graphics.createParticleSystem(glowTex, 300, [0.3, 0.8], {
    space: 'screen',
    facing: 'view',
    emissionRate: 0,
    position: [AVATAR_X, 240],
    direction: [0, 0],
    spread: 180,
    speed: [80, 300],
    gravity: [0, 260],
    linearDamping: [1.4, 2.6],
    sizes: [10, 1],
    sizeVariation: 0.5,
    colors: [[0.6, 1, 1, 0.95], [0.4, 0.2, 0.9, 0]],
    blend: 'additive',
});

// ---- audio ---------------------------------------------------------------
const musicStream = efx.audio.loadAudioStream('music.wav');
const sting = efx.audio.loadAudioData('sting.wav');
let music = null;
let musicVolume = 0.5;
let muted = false;
let audioReady = false;

function unlockAudio() {
    if (audioReady) { return; }
    audioReady = true;
    try { efx.audio.resume(); } catch (e) { /* no audio device */ }
    music = efx.audio.playAudio(musicStream, { loop: true, volume: muted ? 0 : musicVolume });
}
function applyVolume() {
    if (music) { music.volume = muted ? 0 : musicVolume; }
}

// ---- state ---------------------------------------------------------------
// states: 'attract' | 'run' | 'over'
let state = 'attract';
let avatarY = FRAME_H / 2;
let vy = 0;
let gates = [];
let score = 0;
let best = 0;
let spawnX = FRAME_W + 80;
let speed = SPEED_BASE;
let clock = 0;
let restartEdge = false;
let deathX = 0;
let deathY = 0;

function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
function gapHeight() { return clamp(GATE_GAP_MAX - score * 3.2, GATE_GAP_MIN, GATE_GAP_MAX); }

function spawnGate(x) {
    const gh = gapHeight();
    const margin = 34;
    const gapY = margin + Math.random() * (FRAME_H - gh - margin * 2);
    gates.push({ x: x, gapY: gapY, gapH: gh, passed: false });
}

function startRun() {
    state = 'run';
    avatarY = FRAME_H / 2;
    vy = 0;
    gates = [];
    score = 0;
    speed = SPEED_BASE;
    spawnX = FRAME_W + 120;
    clock = 0;
}

function wake() {
    if (state !== 'attract') { return; }
    unlockAudio();
    startRun();
}

function die(x, y) {
    deathX = x;
    deathY = y;
    burst.set({ position: [x, y] });
    burst.emit(46);
    if (audioReady) {
        efx.audio.playAudio(sting, { volume: 0.9, pitch: 1 });
    }
    if (state === 'run') {
        best = Math.max(best, score);
    }
    state = 'over';
    restartEdge = false;
}

function restart() {
    if (state === 'attract') { wake(); return; }
    startRun();
}

// ---- input ---------------------------------------------------------------
efx.keyboard.onDown(function (e) {
    restartEdge = true;
    if (state === 'attract') { wake(); return; }
    if (state === 'over') { restart(); return; }
    if (e.key === 'm' && !e.repeat) {
        muted = !muted;
        applyVolume();
    } else if (e.key === '[') {
        musicVolume = clamp(musicVolume - 0.1, 0, 1);
        applyVolume();
    } else if (e.key === ']') {
        musicVolume = clamp(musicVolume + 0.1, 0, 1);
        applyVolume();
    }
});

efx.mouse.onDown(function () {
    restartEdge = true;
    if (state === 'attract') { wake(); }
    else if (state === 'over') { restart(); }
});

function padHeld() {
    const p = efx.gamepad.count > 0 ? efx.gamepad.get(0) : null;
    if (!p) { return false; }
    return p.isDown('south') || p.isDown('east') || p.isDown('west') || p.isDown('north');
}
function padRestartEdge() {
    const p = efx.gamepad.count > 0 ? efx.gamepad.get(0) : null;
    if (!p) { return false; }
    return p.isPressed('south') || p.isPressed('east') || p.isPressed('west') || p.isPressed('north');
}

function held() {
    return efx.keyboard.isDown('space') || efx.mouse.isDown('left') || padHeld();
}

// ---- simulation ----------------------------------------------------------
function avatarRect() {
    return { x: AVATAR_X, y: avatarY - AVATAR_H / 2, w: AVATAR_W, h: AVATAR_H };
}

function overlaps(a, bx, by, bw, bh) {
    return a.x + a.w > bx && a.x < bx + bw && a.y + a.h > by && a.y < by + bh;
}

function stepAvatar(dt, thrust) {
    vy += (thrust ? -THRUST : GRAVITY) * dt;
    vy = clamp(vy, -VY_MAX, VY_MAX);
    avatarY += vy * dt;
    if (avatarY < AVATAR_H / 2) { avatarY = AVATAR_H / 2; vy = Math.max(0, vy); }
    if (avatarY > FRAME_H - AVATAR_H / 2) { avatarY = FRAME_H - AVATAR_H / 2; vy = Math.min(0, vy); }
}

function stepGates(dt) {
    for (const g of gates) { g.x -= speed * dt; }
    while (gates.length && gates[0].x + GATE_W < -20) { gates.shift(); }
    spawnX -= speed * dt;
    if (spawnX <= FRAME_W) {
        spawnGate(FRAME_W + 10);
        spawnX += GATE_SPACING;
    }
}

function checkGates() {
    const a = avatarRect();
    for (const g of gates) {
        if (!g.passed && g.x + GATE_W < AVATAR_X) {
            g.passed = true;
            score++;
            speed = Math.min(SPEED_MAX, SPEED_BASE + score * SPEED_PER_POINT);
        }
        if (g.passed) { continue; }
        // top wall and bottom wall of the gate
        if (overlaps(a, g.x, 0, GATE_W, g.gapY) ||
            overlaps(a, g.x, g.gapY + g.gapH, GATE_W, FRAME_H - (g.gapY + g.gapH))) {
            die(AVATAR_X + AVATAR_W / 2, avatarY);
            return;
        }
    }
}

function nextGate() {
    for (const g of gates) {
        if (g.x + GATE_W > AVATAR_X) { return g; }
    }
    return null;
}

// ---- update --------------------------------------------------------------
function update(dt) {
    if (dt > 0.05) { dt = 0.05; }
    clock += dt;

    if (state === 'attract') {
        const g = nextGate();
        const target = g ? g.gapY + g.gapH / 2 : FRAME_H * 0.5;
        const wobble = Math.sin(clock * 2.3) * 24;
        stepAvatar(dt, avatarY > target + wobble);
        stepGates(dt);
        checkGates();
        if (state === 'over') {
            // attract never stops: immediately restart the demo
            startRun();
            state = 'attract';
            gates = [];
            spawnX = FRAME_W + 120;
        }
        return;
    }

    if (state === 'run') {
        stepAvatar(dt, held());
        stepGates(dt);
        checkGates();
    } else if (state === 'over') {
        if (padRestartEdge() && !restartEdge) { restartEdge = true; }
        if (restartEdge) { restart(); }
    }
    restartEdge = false;
}

// ---- render --------------------------------------------------------------
function bar(x, y, w, h, c) {
    efx.graphics.drawQuad(efx.graphics.whiteTexture, x, y, { size: [w, h], origin: [0, 0], color: c });
}

function render() {
    efx.graphics.setBlendMode('alpha');

    for (const g of gates) {
        bar(g.x, 0, GATE_W, g.gapY, COL_GATE);
        bar(g.x, g.gapY + g.gapH, GATE_W, FRAME_H - (g.gapY + g.gapH), COL_GATE);
        // edge highlights
        bar(g.x, g.gapY - 3, GATE_W, 3, [1, 1, 1, 0.7]);
        bar(g.x, g.gapY + g.gapH, GATE_W, 3, [1, 1, 1, 0.7]);
    }

    efx.graphics.drawParticles(burst);

    // avatar glow + core
    efx.graphics.setBlendMode('additive');
    efx.graphics.drawQuad(glowTex, AVATAR_X + AVATAR_W / 2 - 42, avatarY - 42, {
        size: [84, 84], color: [COL_AVATAR[0], COL_AVATAR[1], COL_AVATAR[2], 0.7],
    });
    efx.graphics.setBlendMode('alpha');
    bar(AVATAR_X, avatarY - AVATAR_H / 2, AVATAR_W, AVATAR_H, COL_AVATAR);

    efx.graphics.drawText(String(score), bigFont, 320, 18, { align: 'center', color: COL_TEXT });
    efx.graphics.drawText('BEST ' + best, hudFont, FRAME_W - 16, 20, { align: 'right', color: [0.6, 0.7, 0.9, 0.9] });

    if (state === 'attract') {
        efx.graphics.drawText('GLOW GAUNTLET', bigFont, 320, 168, {
            align: 'center', color: [1, 1, 1, 0.92], outlineColor: [0.5, 0.2, 0.9, 1],
        });
        const a = 0.4 + 0.35 * Math.sin(clock * 3.0);
        efx.graphics.drawText('hold to rise — any key / click / button to play', hudFont, 320, 236, {
            align: 'center', color: [0.7, 0.85, 1, a],
        });
    } else if (state === 'over') {
        efx.graphics.drawText('CRASHED', bigFont, 320, 168, { align: 'center', color: [1, 0.45, 0.5, 1] });
        efx.graphics.drawText('score ' + score + '   best ' + best, hudFont, 320, 226, { align: 'center', color: COL_TEXT });
        efx.graphics.drawText('press any key / click / button to retry', hudFont, 320, 258, {
            align: 'center', color: [0.8, 0.85, 1, 0.9],
        });
    }
}
