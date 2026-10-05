// Gamepad Tester — an F13 tour of efx.gamepad. Plug in a controller and watch
// every semantic button, stick, trigger, and d-pad element light up live, with
// the pad name, mapping status, and connect/disconnect events. Unmapped pads
// fall back to a raw axes/buttons panel. Keys 1-4 pick a slot; Tab cycles
// connected slots. Procedural 2D drawing; the only asset is the sample's CC0
// font.

efx.graphics.setClearColor([0.04, 0.05, 0.08, 1]);
efx.graphics.setCamera2D({ frame: [800, 520] });

const titleFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 30, { outline: { width: 2 }, shadow: { blur: 3, offset: [2, 2] } });
const font = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 15);
const small = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 11);

const W = 800;
const H = 520;
const PANEL = [0.075, 0.085, 0.12, 1];
const TRACK = [0.14, 0.16, 0.22, 1];
const DIM = [0.16, 0.18, 0.25, 1];
const LIT = [0.42, 0.92, 1.0, 1];
const WARM = [1.0, 0.66, 0.28, 1];
const MINT = [0.45, 1.0, 0.62, 1];

const BUTTON_NAMES = [
    'south', 'east', 'west', 'north', 'leftShoulder', 'rightShoulder',
    'leftTrigger', 'rightTrigger', 'back', 'start', 'guide', 'leftStick',
    'rightStick', 'dpadUp', 'dpadDown', 'dpadLeft', 'dpadRight',
];

let selected = 0;
let events = [];
let t = 0;

efx.gamepad.onConnect(function (p) {
    events.unshift('CONNECT  #' + p.index + '  ' + (p.name || 'unknown'));
    events = events.slice(0, 5);
    selected = p.index;
});
efx.gamepad.onDisconnect(function (p) {
    events.unshift('DISCONNECT  #' + p.index + '  ' + (p.name || 'unknown'));
    events = events.slice(0, 5);
});
efx.keyboard.onDown(function (e) {
    if (e.key >= '1' && e.key <= '4') {
        selected = e.key.charCodeAt(0) - 49;
    } else if (e.key === 'tab') {
        for (let k = 1; k <= 4; k++) {
            const i = (selected + k) % 4;
            if (efx.gamepad.get(i)) {
                selected = i;
                break;
            }
        }
    }
});

// ---------------------------------------------------------------- drawing

function rect(x, y, w, h, c, a) {
    if (w <= 0 || h <= 0) {
        return; // drawQuad rejects non-positive sizes (e.g. an at-rest trigger)
    }
    efx.graphics.drawQuad(efx.graphics.whiteTexture, x, y, {
        size: [w, h],
        origin: [0, 0],
        color: [c[0], c[1], c[2], a === undefined ? 1 : a],
    });
}

function disc(cx, cy, r, c, a) {
    const steps = Math.max(4, Math.round(r * 1.4));
    for (let i = 0; i < steps; i++) {
        const y0 = -r + (2 * r) * (i / steps);
        const y1 = -r + (2 * r) * ((i + 1) / steps);
        const ym = (y0 + y1) / 2;
        const half = Math.sqrt(Math.max(0, r * r - ym * ym));
        rect(cx - half, cy + y0, half * 2, (y1 - y0) + 0.6, c, a);
    }
}

function txt(s, x, y, f, opts) {
    const o = opts || {};
    // drawText colors are RGBA; tolerate a 3-component RGB palette entry
    if (o.color) o.color = rgba(o.color);
    if (o.outlineColor) o.outlineColor = rgba(o.outlineColor);
    if (o.shadowColor) o.shadowColor = rgba(o.shadowColor);
    efx.graphics.drawText(String(s), f, x, y, o);
}

function rgba(c) {
    return c.length === 4 ? c : [c[0], c[1], c[2], 1];
}

function label(s, x, y, c) {
    txt(s, x, y, small, { align: 'center', color: c || [0.62, 0.7, 0.85, 1] });
}

// -------------------------------------------------------------- controls

function pad() {
    return efx.gamepad.get(selected);
}

function drawTrigger(x, y, w, h, name, caption) {
    const p = pad();
    const v = p ? p.axis(name) : 0;
    const down = p && p.isDown(name);
    rect(x, y, w, h, TRACK, 1);
    const f = Math.max(0, Math.min(1, v));
    rect(x, y + h * (1 - f), w, h * f, down ? WARM : LIT, 1);
    label(caption, x + w / 2, y + h + 4);
    txt(v.toFixed(2), x + w / 2, y - 15, small, {
        align: 'center',
        color: down ? WARM : [0.82, 0.9, 1, 1],
    });
}

function drawShoulder(x, y, w, h, name, caption) {
    const p = pad();
    const down = p && p.isDown(name);
    rect(x, y, w, h, down ? WARM : DIM, 1);
    label(caption, x + w / 2, y + 5, down ? [0.1, 0.08, 0.05, 1] : [0.8, 0.86, 0.95, 1]);
}

function drawStick(cx, cy, r, ax, ay, name, caption) {
    const p = pad();
    rect(cx - r - 4, cy - r - 4, (r + 4) * 2, (r + 4) * 2, PANEL, 1);
    disc(cx, cy, r, TRACK, 1);
    disc(cx, cy, r - 4, PANEL, 1);
    rect(cx - 0.75, cy - r + 4, 1.5, (r - 4) * 2, [0.24, 0.28, 0.38], 0.7);
    rect(cx - r + 4, cy - 0.75, (r - 4) * 2, 1.5, [0.24, 0.28, 0.38], 0.7);
    const down = p && p.isDown(name);
    if (down) {
        disc(cx, cy, r - 2, LIT, 0.22);
    }
    const x = p ? p.axis(ax) : 0;
    const y = p ? p.axis(ay) : 0;
    const px = cx + Math.max(-1, Math.min(1, x)) * (r - 7);
    const py = cy + Math.max(-1, Math.min(1, -y)) * (r - 7);
    disc(px, py, 6.5, down ? WARM : LIT, 1);
    label(caption, cx, cy + r + 6);
    txt(x.toFixed(2) + '  ' + y.toFixed(2), cx, cy + r + 20, small, {
        align: 'center',
        color: [0.7, 0.78, 0.9, 1],
    });
}

function drawFace(cx, cy, r, name, caption, color) {
    const p = pad();
    const down = p && p.isDown(name);
    disc(cx, cy, r, down ? color : DIM, 1);
    disc(cx, cy, r - 2, down ? color : [0.09, 0.1, 0.14], 1);
    label(caption, cx, cy - 6, down ? [0.05, 0.05, 0.08, 1] : color);
}

function drawDpad(cx, cy, s) {
    const p = pad();
    const arm = s * 0.42;
    const hit = function (name, dx, dy) {
        const down = p && p.isDown(name);
        rect(cx + dx * s - arm, cy + dy * s - arm, arm * 2, arm * 2,
             down ? LIT : DIM, 1);
    };
    rect(cx - arm, cy - arm, arm * 2, arm * 2, [0.1, 0.11, 0.15], 1);
    hit('dpadUp', 0, -1);
    hit('dpadDown', 0, 1);
    hit('dpadLeft', -1, 0);
    hit('dpadRight', 1, 0);
    label('D-PAD', cx, cy + s + 8);
}

function drawPill(cx, cy, w, h, name, caption) {
    const p = pad();
    const down = p && p.isDown(name);
    rect(cx - w / 2, cy - h / 2, w, h, down ? MINT : DIM, 1);
    label(caption, cx, cy - 5, down ? [0.04, 0.08, 0.05, 1] : [0.8, 0.86, 0.95, 1]);
}

function drawCard(x, y, w, h, i) {
    const p = efx.gamepad.get(i);
    const sel = i === selected;
    rect(x, y, w, h, sel ? [0.1, 0.15, 0.24] : [0.07, 0.08, 0.11], 1);
    if (sel) {
        rect(x, y, w, 2, LIT, 1);
    }
    txt('#' + i, x + 8, y + 5, small, { color: [0.6, 0.7, 0.85, 1] });
    if (p) {
        txt(p.name || 'UNKNOWN', x + 8, y + 22, small, {
            color: [0.9, 0.95, 1, 1], width: w - 16,
        });
        txt(p.mapped ? 'MAPPED' : 'RAW', x + 8, y + 38, small, {
            color: p.mapped ? MINT : WARM,
        });
        disc(x + w - 16, y + 14, 5, p.mapped ? MINT : WARM, 1);
    } else {
        txt('EMPTY', x + 8, y + 22, small, { color: [0.4, 0.45, 0.55, 1] });
    }
}

function drawRawPanel(x, y, w) {
    const p = pad();
    label('RAW FALLBACK (no mapping)', x + w / 2, y);
    const cols = 16;
    const bw = 12;
    const gap = 4;
    const rowW = cols * (bw + gap) - gap;
    const x0 = x + (w - rowW) / 2;
    for (let i = 0; i < 32; i++) {
        const on = p && p.rawButton(i);
        const rx = x0 + (i % cols) * (bw + gap);
        const ry = y + 16 + Math.floor(i / cols) * (bw + gap);
        rect(rx, ry, bw, bw, on ? LIT : DIM, 1);
    }
    for (let a = 0; a < 6; a++) {
        const v = p ? p.rawAxis(a) : 0;
        const bx = x0 + a * (bw + gap) * 2;
        const by = y + 50;
        rect(bx, by, bw * 2 + gap, 10, TRACK, 1);
        const f = Math.max(0, Math.min(1, (v + 1) * 0.5));
        rect(bx, by, (bw * 2 + gap) * f, 10, LIT, 1);
    }
}

// ---------------------------------------------------------------- render

efx.registerRenderHook(function () {
    txt('GAMEPAD TESTER', W / 2, 8, titleFont, {
        align: 'center',
        color: [1, 0.85, 0.2, 1],
        outlineColor: [0.95, 0.25, 0.05, 1],
        shadowColor: [0, 0, 0, 0.85],
    });
    txt('PADS: ' + efx.gamepad.count + '   (1-4 select, Tab cycle)',
        W / 2, 44, font, { align: 'center', color: [0.8, 0.88, 1, 1] });

    const cw = 185;
    const gap = 10;
    const cx0 = (W - (4 * cw + 3 * gap)) / 2;
    for (let i = 0; i < 4; i++) {
        drawCard(cx0 + i * (cw + gap), 70, cw, 54, i);
    }

    const p = pad();

    // controller panel
    const px = 70;
    const py = 138;
    const pw = W - 140;
    const ph = 260;
    rect(px, py, pw, ph, PANEL, 1);

    // triggers + shoulders
    drawTrigger(px + 18, py + 20, 40, 96, 'leftTrigger', 'LT');
    drawTrigger(px + pw - 58, py + 20, 40, 96, 'rightTrigger', 'RT');
    drawShoulder(px + 72, py + 20, 116, 26, 'leftShoulder', 'LB');
    drawShoulder(px + pw - 188, py + 20, 116, 26, 'rightShoulder', 'RB');

    // sticks
    drawStick(px + 140, py + 168, 56, 'leftX', 'leftY', 'leftStick', 'LEFT STICK');
    drawStick(px + pw - 140, py + 168, 56, 'rightX', 'rightY', 'rightStick', 'RIGHT STICK');

    // d-pad and face buttons
    drawDpad(px + pw / 2 - 96, py + 150, 24);
    drawFace(px + pw / 2 + 96, py + 118, 19, 'north', 'Y', [0.95, 0.85, 0.2, 1]);
    drawFace(px + pw / 2 + 66, py + 150, 19, 'west', 'X', [0.4, 0.7, 1.0, 1]);
    drawFace(px + pw / 2 + 126, py + 150, 19, 'east', 'B', [1.0, 0.4, 0.4, 1]);
    drawFace(px + pw / 2 + 96, py + 182, 19, 'south', 'A', [0.45, 1.0, 0.55, 1]);

    // center cluster
    drawPill(px + pw / 2, py + 44, 62, 20, 'guide', 'GUIDE');
    drawPill(px + pw / 2 - 42, py + 76, 52, 18, 'back', 'BACK');
    drawPill(px + pw / 2 + 42, py + 76, 52, 18, 'start', 'START');
    label('L3 / R3 = stick click', px + pw / 2, py + 100);

    if (!p) {
        const pulse = 0.35 + 0.25 * Math.sin(t * 3.0);
        rect(px + 4, py + 4, pw - 8, ph - 8, LIT, pulse * 0.25);
        txt('CONNECT A GAMEPAD', W / 2, py + ph / 2 - 16, font, {
            align: 'center', color: [0.85, 0.92, 1, 1],
        });
        txt('press a button or move a stick', W / 2, py + ph / 2 + 8, small, {
            align: 'center', color: [0.6, 0.7, 0.85, 1],
        });
    } else if (!p.mapped) {
        drawRawPanel(px, py + ph + 8, pw);
    }

    // event log
    label('EVENTS', W - 150, H - 62);
    for (let i = 0; i < events.length; i++) {
        txt(events[i], W - 280, H - 46 + i * 12, small, {
            color: [0.7, 0.78, 0.9, 1 - i * 0.15],
        });
    }
    txt('built-in gamepad test — F13', W / 2, H - 14, small, {
        align: 'center', color: [0.4, 0.46, 0.58, 1],
    });
});

efx.registerUpdateHook(function (dt) {
    if (dt > 0.05) {
        dt = 0.05;
    }
    t += dt;
});
