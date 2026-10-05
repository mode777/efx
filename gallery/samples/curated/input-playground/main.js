// Input Playground — an interactive F9 tour. Move the pointer to paint a
// glowing trail, click to burst, turn the wheel to resize the brush, press
// 1-8 for a palette, space for a nova, C to clear, and hold WASD/arrows or
// Shift to steer. It self-plays until you touch it. Uses the mouse event API,
// keyboard events + queries, mouse/window read-only properties, and the
// surface-pixel -> 2D-frame mapping the docs describe.

efx.graphics.setClearColor([0.02, 0.016, 0.05, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const FRAME_W = 640;
const FRAME_H = 480;

// ---- deterministic-ish little RNG (visual only; no host dependency) ----
let rngState = 0x2f6e2b1;
function rnd() {
    rngState = (rngState * 1103515245 + 12345) & 0x7fffffff;
    return rngState / 0x7fffffff;
}
function rand(a, b) {
    return a + (b - a) * rnd();
}

const PALETTE = [
    [1.0, 0.25, 0.35],
    [1.0, 0.58, 0.20],
    [1.0, 0.93, 0.35],
    [0.35, 1.0, 0.55],
    [0.30, 0.85, 1.0],
    [0.52, 0.55, 1.0],
    [0.92, 0.42, 1.0],
    [1.0, 1.0, 1.0],
];
let colorIndex = 4;

// ---- particle pool: fixed cap, overwrite-oldest ring ----
const MAX = 400;
const pParts = [];
for (let i = 0; i < MAX; i++) {
    pParts.push({ on: false, x: 0, y: 0, vx: 0, vy: 0, life: 0, max: 1, size: 2, r: 1, g: 1, b: 1 });
}
let pNext = 0;

function spawn(x, y, vx, vy, size, life, c) {
    const p = pParts[pNext];
    pNext = (pNext + 1) % MAX;
    p.on = true;
    p.x = x;
    p.y = y;
    p.vx = vx;
    p.vy = vy;
    p.life = life;
    p.max = life;
    p.size = size;
    p.r = c[0];
    p.g = c[1];
    p.b = c[2];
}

// ---- input state ----
let winW = FRAME_W;
let winH = FRAME_H;
function readWindow() {
    const s = efx.window.size;
    if (s[0] > 0) {
        winW = s[0];
    }
    if (s[1] > 0) {
        winH = s[1];
    }
}
// mouse.surface px -> 640x480 frame px (the documented hit-test rule)
function toFrame(x, y) {
    return [x * (FRAME_W / winW), y * (FRAME_H / winH)];
}

let pointerX = FRAME_W * 0.5;
let pointerY = FRAME_H * 0.5;
let havePointer = false;
let lastX = pointerX;
let lastY = pointerY;
let brush = 1;
let attract = true;
let hint = 0;

function wake() {
    attract = false;
}

function nova(x, y) {
    const c = PALETTE[colorIndex];
    const n = 64;
    for (let i = 0; i < n; i++) {
        const a = (i / n) * Math.PI * 2 + rnd() * 0.2;
        const sp = rand(70, 170) * brush;
        spawn(x, y, Math.cos(a) * sp, Math.sin(a) * sp, rand(2, 5) * brush,
              rand(0.5, 1.1), c);
    }
}

function clearParts() {
    for (let i = 0; i < MAX; i++) {
        pParts[i].on = false;
    }
}

// ---- event API: mouse drives discrete strokes/bursts ----
efx.mouse.onMove(function (e) {
    const f = toFrame(e.x, e.y);
    pointerX = f[0];
    pointerY = f[1];
    havePointer = true;
    wake();
    // paint along the segment the pointer swept this event
    const dx = pointerX - lastX;
    const dy = pointerY - lastY;
    const dist = Math.hypot(dx, dy);
    const steps = Math.min(6, Math.max(1, Math.ceil(dist / 6)));
    const c = PALETTE[colorIndex];
    for (let i = 0; i < steps; i++) {
        const t = i / steps;
        spawn(lastX + dx * t, lastY + dy * t, -dx * 2.2 + rand(-14, 14),
              -dy * 2.2 + rand(-14, 14), rand(2.0, 3.6) * brush,
              rand(0.35, 0.8), c);
    }
    lastX = pointerX;
    lastY = pointerY;
});

efx.mouse.onDown(function (e) {
    const f = toFrame(e.x, e.y);
    pointerX = f[0];
    pointerY = f[1];
    havePointer = true;
    lastX = pointerX;
    lastY = pointerY;
    wake();
    if (e.button === 'left') {
        nova(pointerX, pointerY);
    }
});

efx.mouse.onWheel(function (e) {
    brush = Math.max(0.4, Math.min(3.0, brush * (1 - e.dy * 0.09)));
    // the wheel also sweeps the palette so it is visible without keys
    if (e.dy > 0) {
        colorIndex = (colorIndex + 1) % PALETTE.length;
    } else if (e.dy < 0) {
        colorIndex = (colorIndex + PALETTE.length - 1) % PALETTE.length;
    }
    wake();
});

// ---- event API: one-shot keys carry the auto-repeat flag ----
efx.keyboard.onDown(function (e) {
    if (e.repeat) {
        return;
    }
    if (e.key === 'space') {
        nova(pointerX, pointerY);
    } else if (e.key === 'c') {
        clearParts();
    } else if (e.key >= '1' && e.key <= '8') {
        colorIndex = e.key.charCodeAt(0) - 49;
    }
    wake();
});

// ---- update: continuous behavior polls held state ----
let t = 0;
function update(dt) {
    if (dt > 0.05) {
        dt = 0.05;
    }
    t += dt;
    readWindow();

    if (attract) {
        hint += dt;
        // self-playing wanderer paints while nobody is interacting
        const a = t * 0.9;
        const ex = FRAME_W * 0.5 + Math.cos(a * 1.3) * 190;
        const ey = FRAME_H * 0.5 + Math.sin(a * 1.7) * 120;
        const c = PALETTE[(Math.floor(t * 0.5) + 4) % PALETTE.length];
        for (let i = 0; i < 3; i++) {
            spawn(ex, ey, -Math.sin(a * 1.3) * 30 + rand(-40, 40),
                  Math.cos(a * 1.7) * 30 + rand(-40, 40), rand(2, 4),
                  rand(0.6, 1.2), c);
        }
    }

    // held direction keys = wind force; held shift = bigger brush
    let wx = 0;
    let wy = 0;
    if (efx.keyboard.isDown('left') || efx.keyboard.isDown('a')) {
        wx -= 240;
    }
    if (efx.keyboard.isDown('right') || efx.keyboard.isDown('d')) {
        wx += 240;
    }
    if (efx.keyboard.isDown('up') || efx.keyboard.isDown('w')) {
        wy -= 240;
    }
    if (efx.keyboard.isDown('down') || efx.keyboard.isDown('s')) {
        wy += 240;
    }
    const big = efx.keyboard.isDown('lshift') || efx.keyboard.isDown('rshift') ? 1.7 : 1;

    // held mouse buttons are gravity wells at the pointer
    let well = 0;
    if (havePointer) {
        if (efx.mouse.isDown('left')) {
            well = 1;
        } else if (efx.mouse.isDown('right')) {
            well = -1;
        }
    }

    for (let i = 0; i < MAX; i++) {
        const p = pParts[i];
        if (!p.on) {
            continue;
        }
        p.life -= dt;
        if (p.life <= 0) {
            p.on = false;
            continue;
        }
        if (well !== 0) {
            const dx = pointerX - p.x;
            const dy = pointerY - p.y;
            const d2 = dx * dx + dy * dy + 60;
            const f = (well * 2600) / d2;
            p.vx += dx * f * dt;
            p.vy += dy * f * dt;
        }
        p.vx += wx * dt;
        p.vy += wy * dt;
        const drag = 1 - Math.min(1, dt * 1.6);
        p.vx *= drag;
        p.vy *= drag;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (big > 1 && attract === false) {
            p.size += (big * 3.2 - p.size) * Math.min(1, dt * 4);
        }
        if (p.x < -20) { p.x = FRAME_W + 20; }
        if (p.x > FRAME_W + 20) { p.x = -20; }
        if (p.y < -20) { p.y = FRAME_H + 20; }
        if (p.y > FRAME_H + 20) { p.y = -20; }
    }
}

function bar(x, y, w, h, c, a) {
    efx.graphics.drawQuad(efx.graphics.whiteTexture, x, y, {
        size: [w, h],
        origin: [0, 0],
        color: [c[0], c[1], c[2], a],
    });
}

function render() {
    // blend is frame-local render state, so set it at the top of the frame
    efx.graphics.setBlendMode('additive');
    // particles
    for (let i = 0; i < MAX; i++) {
        const p = pParts[i];
        if (!p.on) {
            continue;
        }
        const f = p.life / p.max;
        const s = p.size * (0.6 + 0.4 * f);
        efx.graphics.drawQuad(efx.graphics.whiteTexture, p.x, p.y, {
            size: [s, s],
            color: [p.r, p.g, p.b, 0.85 * f * f],
        });
    }

    // palette swatches along the bottom; the selected one is ringed
    const n = PALETTE.length;
    const sw = 26;
    const gap = 8;
    const totalW = n * sw + (n - 1) * gap;
    const x0 = FRAME_W * 0.5 - totalW * 0.5;
    const y0 = FRAME_H - 26;
    for (let i = 0; i < n; i++) {
        const x = x0 + i * (sw + gap);
        const sel = i === colorIndex;
        if (sel) {
            bar(x - 5, y0 - 5, sw + 10, sw + 10, [1, 1, 1], 0.9);
        }
        bar(x, y0, sw, sw, PALETTE[i], sel ? 0.95 : 0.5);
    }

    // pointer crosshair
    if (havePointer && !attract) {
        const c = PALETTE[colorIndex];
        bar(pointerX - 12, pointerY - 0.75, 24, 1.5, c, 0.9);
        bar(pointerX - 0.75, pointerY - 12, 1.5, 24, c, 0.9);
    }

    // attract hint: a pulsing frame border inviting a click
    if (attract) {
        const a = 0.25 + 0.25 * Math.sin(hint * 3.0);
        bar(0, 0, FRAME_W, 2, [0.5, 0.8, 1.0], a);
        bar(0, FRAME_H - 2, FRAME_W, 2, [0.5, 0.8, 1.0], a);
        bar(0, 0, 2, FRAME_H, [0.5, 0.8, 1.0], a);
        bar(FRAME_W - 2, 0, 2, FRAME_H, [0.5, 0.8, 1.0], a);
    }
}
