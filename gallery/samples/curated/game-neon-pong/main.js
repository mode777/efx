// Neon Pong — rung 1 of the game gallery series. A complete Pong match:
// serve, rally, score, first to 7, restart. Mouse-Y or up/down drive the
// player paddle; an imperfect AI opposes it; P toggles two-player. It
// self-plays (both paddles AI) until the first input, so the gallery embed
// shows live gameplay. 2D quads + text + input only — no physics, particles,
// audio, or post effects.

efx.graphics.setClearColor([0.02, 0.02, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const FRAME_W = 640;
const FRAME_H = 480;

const PADDLE_W = 10;
const PADDLE_H = 72;
const PADDLE_MARGIN = 26;
const PADDLE_SPEED = 420; // keyboard / AI cap, px per second

const BALL = 12;
const BALL_SPEED_START = 300;
const BALL_SPEEDUP = 1.035;
const BALL_SPEED_MAX = 640;
const MAX_BOUNCE_DEG = 55;

const SERVE_DELAY = 0.9;
const POINT_PAUSE = 0.55;
const TARGET = 7;

const LEFT_X = PADDLE_MARGIN;
const RIGHT_X = FRAME_W - PADDLE_MARGIN - PADDLE_W;

const COL_BG = [0.02, 0.02, 0.06, 1];
const COL_LEFT = [0.25, 0.95, 1.0, 1];
const COL_RIGHT = [1.0, 0.3, 0.75, 1];
const COL_BALL = [1.0, 0.95, 0.7, 1];
const COL_LINE = [0.15, 0.3, 0.5, 0.5];

// ---- fonts ---------------------------------------------------------------
const scoreFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 56, {
    outline: { width: 2 },
    shadow: { blur: 4, offset: [2, 2] },
});
const hintFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 20);

// ---- a soft radial glow texture (procedural; no assets) ------------------
function makeGlow(size) {
    const px = [];
    const r = size / 2;
    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const dx = (x + 0.5 - r) / r;
            const dy = (y + 0.5 - r) / r;
            const d = Math.sqrt(dx * dx + dy * dy);
            const a = Math.max(0, 1 - d);
            const v = Math.round(a * a * a * 255);
            px.push(255, 255, 255, v);
        }
    }
    return efx.graphics.createTexture(efx.graphics.createImageData(size, size, px), { filter: 'linear' });
}
const glow = makeGlow(64);

// ---- state ---------------------------------------------------------------
// states: 'attract' | 'serve' | 'point' | 'rally' | 'win'
let state = 'attract';
let twoPlayer = false;
let leftY = (FRAME_H - PADDLE_H) / 2;
let rightY = (FRAME_H - PADDLE_H) / 2;
let ballX = FRAME_W / 2 - BALL / 2;
let ballY = FRAME_H / 2 - BALL / 2;
let ballVX = 0;
let ballVY = 0;
let ballSpeed = BALL_SPEED_START;
let leftScore = 0;
let rightScore = 0;
let timer = 0;
let serveDir = 1; // +1 toward right player, -1 toward left
let pointerY = FRAME_H / 2;
let pointerMoved = false;
let winFlash = 0;

function clamp(v, a, b) {
    return v < a ? a : (v > b ? b : v);
}

function resetBall() {
    ballX = FRAME_W / 2 - BALL / 2;
    ballY = FRAME_H / 2 - BALL / 2;
    ballVX = 0;
    ballVY = 0;
    ballSpeed = BALL_SPEED_START;
}

function beginServe(dir) {
    resetBall();
    serveDir = dir;
    state = 'serve';
    timer = SERVE_DELAY;
}

function startMatch(dir) {
    leftScore = 0;
    rightScore = 0;
    leftY = (FRAME_H - PADDLE_H) / 2;
    rightY = (FRAME_H - PADDLE_H) / 2;
    beginServe(dir);
}

function wake() {
    if (state !== 'attract') {
        return;
    }
    state = 'serve';
    twoPlayer = false;
    startMatch(1); // hand the player the first serve
}

function restart() {
    startMatch(Math.random() < 0.5 ? 1 : -1);
}

// ---- input ---------------------------------------------------------------
efx.keyboard.onDown(function (e) {
    if (state === 'attract') {
        wake();
        return;
    }
    if (state === 'win') {
        restart();
        return;
    }
    if (e.key === 'p' && !e.repeat) {
        twoPlayer = !twoPlayer;
    }
});

efx.mouse.onDown(function () {
    if (state === 'attract') {
        wake();
    } else if (state === 'win') {
        restart();
    }
});

efx.mouse.onMove(function (e) {
    const s = efx.window.size;
    const fh = s[1] > 0 ? s[1] : FRAME_H;
    pointerY = e.y * (FRAME_H / fh);
    pointerMoved = true;
    if (state === 'attract') {
        wake();
    }
});

function paddleInput(dt) {
    // right paddle: player (mouse or up/down) in 1P; up/down in 2P
    if (!twoPlayer) {
        const up = efx.keyboard.isDown('up');
        const down = efx.keyboard.isDown('down');
        if (up || down) {
            rightY += (down ? 1 : -1) * PADDLE_SPEED * dt;
            pointerMoved = false;
        } else if (pointerMoved) {
            rightY = pointerY - PADDLE_H / 2;
        }
    } else {
        if (efx.keyboard.isDown('up')) { rightY -= PADDLE_SPEED * dt; }
        if (efx.keyboard.isDown('down')) { rightY += PADDLE_SPEED * dt; }
    }
    rightY = clamp(rightY, 0, FRAME_H - PADDLE_H);
}

function aiPaddle(y, dt, error) {
    const target = ballY + BALL / 2 - PADDLE_H / 2 + error;
    const diff = target - y;
    const step = clamp(diff, -PADDLE_SPEED * dt, PADDLE_SPEED * dt);
    return clamp(y + step, 0, FRAME_H - PADDLE_H);
}

// ---- physics -------------------------------------------------------------
function bounceOffPaddle(paddleY, fromLeft) {
    const ballCY = ballY + BALL / 2;
    const rel = clamp((ballCY - (paddleY + PADDLE_H / 2)) / (PADDLE_H / 2), -1, 1);
    const angle = rel * (MAX_BOUNCE_DEG * Math.PI / 180);
    ballSpeed = Math.min(ballSpeed * BALL_SPEEDUP, BALL_SPEED_MAX);
    const dir = fromLeft ? 1 : -1;
    ballVX = Math.cos(angle) * ballSpeed * dir;
    ballVY = Math.sin(angle) * ballSpeed;
}

function moveBall(dt) {
    // substep so the ball cannot tunnel through a paddle
    const dist = Math.hypot(ballVX, ballVY) * dt;
    const steps = Math.max(1, Math.ceil(dist / (BALL * 0.4)));
    const h = dt / steps;
    for (let i = 0; i < steps; i++) {
        ballX += ballVX * h;
        ballY += ballVY * h;

        if (ballY < 0) { ballY = 0; ballVY = Math.abs(ballVY); }
        if (ballY + BALL > FRAME_H) { ballY = FRAME_H - BALL; ballVY = -Math.abs(ballVY); }

        // left paddle
        if (ballVX < 0 && ballX <= LEFT_X + PADDLE_W && ballX + BALL >= LEFT_X) {
            if (ballY + BALL > leftY && ballY < leftY + PADDLE_H) {
                ballX = LEFT_X + PADDLE_W;
                bounceOffPaddle(leftY, true);
            }
        }
        // right paddle
        if (ballVX > 0 && ballX + BALL >= RIGHT_X && ballX <= RIGHT_X + PADDLE_W) {
            if (ballY + BALL > rightY && ballY < rightY + PADDLE_H) {
                ballX = RIGHT_X - BALL;
                bounceOffPaddle(rightY, false);
            }
        }

        if (ballX + BALL < 0) { scorePoint(false); return; }
        if (ballX > FRAME_W) { scorePoint(true); return; }
    }
}

function scorePoint(playerScored) {
    if (playerScored) { rightScore++; } else { leftScore++; }
    if (state === 'attract') {
        // attract never ends: roll the scoreboard over at the target
        if (rightScore >= TARGET || leftScore >= TARGET) {
            leftScore = 0;
            rightScore = 0;
        }
        resetBall();
        const a = (Math.random() * 2 - 1) * (MAX_BOUNCE_DEG * Math.PI / 180) * 0.6;
        ballVX = Math.cos(a) * ballSpeed * (playerScored ? -1 : 1);
        ballVY = Math.sin(a) * ballSpeed;
        return;
    }
    if (rightScore >= TARGET || leftScore >= TARGET) {
        state = 'win';
        winFlash = 0;
        return;
    }
    // serve toward the player who lost the point
    serveDir = playerScored ? -1 : 1;
    resetBall();
    state = 'point';
    timer = POINT_PAUSE;
}

// ---- update --------------------------------------------------------------
function update(dt) {
    if (dt > 0.05) { dt = 0.05; }
    winFlash += dt;

    const attract = state === 'attract';

    if (state === 'rally' || state === 'serve' || attract) {
        // left paddle: AI (always in attract/1P; W/S in 2P)
        if (twoPlayer && !attract) {
            if (efx.keyboard.isDown('w')) { leftY -= PADDLE_SPEED * dt; }
            if (efx.keyboard.isDown('s')) { leftY += PADDLE_SPEED * dt; }
            leftY = clamp(leftY, 0, FRAME_H - PADDLE_H);
        } else {
            const err = attract ? Math.sin(winFlash * 3.1) * 22 : Math.sin(winFlash * 2.3) * 14;
            leftY = aiPaddle(leftY, dt, err);
        }
        if (attract) {
            rightY = aiPaddle(rightY, dt, Math.sin(winFlash * 2.7 + 1.3) * 18);
        } else {
            paddleInput(dt);
        }
    }

    if (attract) {
        // keep a live rally running forever
        if (ballVX === 0 && ballVY === 0) {
            resetBall();
            const a = (Math.random() * 2 - 1) * (MAX_BOUNCE_DEG * Math.PI / 180) * 0.6;
            ballVX = Math.cos(a) * ballSpeed * (Math.random() < 0.5 ? 1 : -1);
            ballVY = Math.sin(a) * ballSpeed;
        }
        moveBall(dt);
        return;
    }

    if (state === 'serve' || state === 'point') {
        timer -= dt;
        if (timer <= 0) {
            if (state === 'point') {
                beginServe(serveDir);
            } else {
                const a = (Math.random() * 2 - 1) * (MAX_BOUNCE_DEG * Math.PI / 180) * 0.5;
                ballVX = Math.cos(a) * ballSpeed * serveDir;
                ballVY = Math.sin(a) * ballSpeed;
                state = 'rally';
            }
        }
    } else if (state === 'rally') {
        moveBall(dt);
    }
}

// ---- render --------------------------------------------------------------
function glowQuad(x, y, w, h, color, alpha) {
    efx.graphics.drawQuad(glow, x + w / 2 - w * 1.6, y + h / 2 - h * 1.6, {
        size: [w * 3.2, h * 3.2],
        color: [color[0], color[1], color[2], alpha],
    });
}

function bar(x, y, w, h, c) {
    efx.graphics.drawQuad(efx.graphics.whiteTexture, x, y, {
        size: [w, h],
        origin: [0, 0],
        color: c,
    });
}

function render() {
    efx.graphics.setBlendMode('alpha');

    // dashed center line
    for (let y = 8; y < FRAME_H; y += 34) {
        bar(FRAME_W / 2 - 1, y, 2, 20, COL_LINE);
    }

    // paddles + ball glow (additive), then solid cores
    efx.graphics.setBlendMode('additive');
    glowQuad(LEFT_X, leftY, PADDLE_W, PADDLE_H, COL_LEFT, 0.5);
    glowQuad(RIGHT_X, rightY, PADDLE_W, PADDLE_H, COL_RIGHT, 0.5);
    glowQuad(ballX, ballY, BALL, BALL, COL_BALL, 0.7);

    efx.graphics.setBlendMode('alpha');
    bar(LEFT_X, leftY, PADDLE_W, PADDLE_H, COL_LEFT);
    bar(RIGHT_X, rightY, PADDLE_W, PADDLE_H, COL_RIGHT);
    bar(ballX, ballY, BALL, BALL, COL_BALL);

    // score
    efx.graphics.drawText(String(leftScore), scoreFont, 240, 22, {
        align: 'center', color: COL_LEFT,
    });
    efx.graphics.drawText(String(rightScore), scoreFont, 400, 22, {
        align: 'center', color: COL_RIGHT,
    });

    if (state === 'attract') {
        efx.graphics.drawText('NEON PONG', scoreFont, 320, 190, {
            align: 'center', color: [1, 1, 1, 0.9],
            outlineColor: [0.4, 0.2, 0.8, 1],
        });
        const a = 0.4 + 0.35 * Math.sin(winFlash * 3.0);
        efx.graphics.drawText('move / click to play', hintFont, 320, 268, {
            align: 'center', color: [0.7, 0.85, 1, a],
        });
    } else if (state === 'win') {
        const won = rightScore > leftScore;
        efx.graphics.drawText(won ? 'PLAYER WINS' : 'AI WINS', scoreFont, 320, 190, {
            align: 'center', color: won ? COL_RIGHT : COL_LEFT,
        });
        efx.graphics.drawText('press any key to play again', hintFont, 320, 268, {
            align: 'center', color: [0.7, 0.85, 1, 0.85],
        });
    } else {
        const hint = twoPlayer ? '2P: W/S vs UP/DOWN' : 'mouse or UP/DOWN   |   P = 2P';
        efx.graphics.drawText(hint, hintFont, 320, FRAME_H - 30, {
            align: 'center', color: [0.45, 0.55, 0.7, 0.9],
        });
    }
}
