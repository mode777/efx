// Bloom Breakout — rung 2 of the game gallery series. The first "juice" rung:
// particles, one-shot audio, and post bloom as gameplay feedback. Paddle,
// ball, and a brick grid as 2D quads; score/lives as text; three layouts,
// three lives. It self-plays until the first input.

efx.graphics.setClearColor([0.015, 0.01, 0.04, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

efx.graphics.setPostEffects([
    { effect: 'bloom', threshold: 0.55, strength: 0.85 },
    { effect: 'colorFilter', saturation: 1.1, contrast: 1.05 },
]);

const FRAME_W = 640;
const FRAME_H = 480;

const PADDLE_W = 96;
const PADDLE_H = 13;
const PADDLE_Y = FRAME_H - 42;
const PADDLE_SPEED = 520;

const BALL_R = 7;
const BALL_SPEED_START = 250;
const BALL_SPEED_MAX = 560;
const BALL_SPEEDUP = 6;

const COLS = 11;
const BRICK_W = 48;
const BRICK_H = 20;
const BRICK_GAP = 4;
const BRICK_LEFT = (FRAME_W - (COLS * BRICK_W + (COLS - 1) * BRICK_GAP)) / 2;
const BRICK_TOP = 74;

const LIVES = 3;
const SERVE_DELAY = 0.8;

const HP_COLOR = {
    1: [0.25, 0.9, 1.0, 1],
    2: [1.0, 0.32, 0.9, 1],
    3: [1.0, 0.9, 0.3, 1],
};
const COL_BALL = [1.0, 0.97, 0.8, 1];
const COL_PADDLE = [0.6, 0.95, 1.0, 1];
const COL_TEXT = [0.8, 0.88, 1.0, 1];

const LAYOUTS = [
    [
        '11111111111',
        '11111111111',
        '11111111111',
        '11111111111',
    ],
    [
        '11000000011',
        '11100000111',
        '11110001111',
        '11111011111',
        '11111111111',
    ],
    [
        '33333333333',
        '22222222222',
        '11111111111',
        '22222222222',
        '33333333333',
    ],
];

// ---- fonts ---------------------------------------------------------------
const hudFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 22, {
    outline: { width: 1 },
    shadow: { blur: 3, offset: [1, 1] },
});
const bigFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 46, {
    outline: { width: 2 },
});

// ---- particle system (screen-space; burst on brick death) ----------------
function shardTexture() {
    const size = 16;
    const px = [];
    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const dx = (x + 0.5) / size * 2 - 1;
            const dy = (y + 0.5) / size * 2 - 1;
            const d = Math.min(1, Math.sqrt(dx * dx + dy * dy));
            const a = Math.round((1 - d) * (1 - d) * 255);
            px.push(255, 255, 255, a);
        }
    }
    return efx.graphics.createTexture(efx.graphics.createImageData(size, size, px));
}
const shard = shardTexture();
const burst = efx.graphics.createParticleSystem(shard, 400, [0.25, 0.7], {
    space: 'screen',
    facing: 'view',
    emissionRate: 0,
    position: [320, 200],
    direction: [0, 0],
    spread: 180,
    speed: [60, 220],
    gravity: [0, 240],
    linearDamping: [1.5, 3.0],
    sizes: [9, 1],
    sizeVariation: 0.4,
    colors: [[1, 1, 1, 0.95], [1, 0.5, 0.2, 0]],
    blend: 'additive',
});

// ---- audio ---------------------------------------------------------------
const SFX = {
    paddle: efx.audio.loadAudioData('paddle.wav'),
    brick: efx.audio.loadAudioData('brick.wav'),
    wall: efx.audio.loadAudioData('wall.wav'),
    life: efx.audio.loadAudioData('life.wav'),
};
let audioReady = false;
function unlockAudio() {
    if (audioReady) { return; }
    audioReady = true;
    try { efx.audio.resume(); } catch (e) { /* no audio device */ }
}
function sfx(name, pan, pitch) {
    if (!audioReady) { return; }
    const p = Math.max(-1, Math.min(1, pan || 0));
    const pt = pitch || 1;
    efx.audio.playAudio(SFX[name], { volume: 0.85, pan: p, pitch: pt });
}

// ---- state ---------------------------------------------------------------
// states: 'attract' | 'serve' | 'rally' | 'life' | 'over' | 'win'
let state = 'attract';
let layout = 0;
let bricks = [];
let paddleX = (FRAME_W - PADDLE_W) / 2;
let ballX = FRAME_W / 2;
let ballY = PADDLE_Y - BALL_R;
let ballVX = 0;
let ballVY = 0;
let ballSpeed = BALL_SPEED_START;
let lives = LIVES;
let score = 0;
let timer = 0;
let shake = 0;
let shakeX = 0;
let shakeY = 0;
let pointerX = FRAME_W / 2;
let pointerMoved = false;
let clock = 0;

function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }

function loadLayout(index) {
    const rows = LAYOUTS[index];
    bricks = [];
    for (let r = 0; r < rows.length; r++) {
        for (let c = 0; c < COLS; c++) {
            const ch = rows[r][c] || '0';
            const hp = ch === '0' ? 0 : (ch.charCodeAt(0) - 48);
            if (hp <= 0) { continue; }
            bricks.push({
                x: BRICK_LEFT + c * (BRICK_W + BRICK_GAP),
                y: BRICK_TOP + r * (BRICK_H + BRICK_GAP),
                hp: hp,
                maxHp: hp,
            });
        }
    }
}

function bricksLeft() {
    for (const b of bricks) { if (b.hp > 0) { return true; } }
    return false;
}

function resetBall() {
    ballX = paddleX + PADDLE_W / 2;
    ballY = PADDLE_Y - BALL_R - 1;
    ballVX = 0;
    ballVY = 0;
}

function launchBall() {
    const a = (-60 + Math.random() * 30) * Math.PI / 180; // up, mild spread
    ballVX = Math.sin(a) * ballSpeed;
    ballVY = -Math.cos(a) * ballSpeed;
    state = 'rally';
}

function startGame() {
    state = 'serve';
    layout = 0;
    lives = LIVES;
    score = 0;
    loadLayout(0);
    paddleX = (FRAME_W - PADDLE_W) / 2;
    ballSpeed = BALL_SPEED_START;
    resetBall();
    timer = SERVE_DELAY;
}

function wake() {
    if (state !== 'attract') { return; }
    unlockAudio();
    startGame();
}

function restart() {
    startGame();
}

// ---- input ---------------------------------------------------------------
efx.keyboard.onDown(function (e) {
    if (state === 'attract') { wake(); return; }
    if (state === 'over' || state === 'win') { restart(); return; }
});

efx.mouse.onDown(function () {
    if (state === 'attract') { wake(); }
    else if (state === 'over' || state === 'win') { restart(); }
});

efx.mouse.onMove(function (e) {
    const s = efx.window.size;
    const fw = s[0] > 0 ? s[0] : FRAME_W;
    pointerX = e.x * (FRAME_W / fw);
    pointerMoved = true;
    if (state === 'attract') { wake(); }
});

function paddleInput(dt) {
    const left = efx.keyboard.isDown('left') || efx.keyboard.isDown('a');
    const right = efx.keyboard.isDown('right') || efx.keyboard.isDown('d');
    if (left || right) {
        paddleX += (right ? 1 : -1) * PADDLE_SPEED * dt;
        pointerMoved = false;
    } else if (pointerMoved) {
        paddleX = pointerX - PADDLE_W / 2;
    }
    paddleX = clamp(paddleX, 0, FRAME_W - PADDLE_W);
}

// ---- collision -----------------------------------------------------------
function addShake(amount) {
    shake = Math.min(14, shake + amount);
}

function burstAt(x, y, color) {
    burst.set({ position: [x, y], colors: [color, [color[0] * 0.6, color[1] * 0.3, color[2] * 0.2, 0]] });
    burst.emit(18);
}

function hitBrick(b, fromTop) {
    b.hp--;
    score += 10 * b.maxHp;
    addShake(b.maxHp >= 2 ? 4 : 2.5);
    burstAt(b.x + BRICK_W / 2, b.y + BRICK_H / 2, HP_COLOR[b.maxHp] || [1, 1, 1, 1]);
    const pan = (b.x + BRICK_W / 2) / FRAME_W * 2 - 1;
    sfx('brick', pan, 0.96 + Math.random() * 0.08);
}

function moveBall(dt) {
    const dist = Math.hypot(ballVX, ballVY) * dt;
    const steps = Math.max(1, Math.ceil(dist / (BALL_R * 0.5)));
    const h = dt / steps;
    for (let i = 0; i < steps; i++) {
        ballX += ballVX * h;
        ballY += ballVY * h;

        if (ballX - BALL_R < 0) { ballX = BALL_R; ballVX = Math.abs(ballVX); sfx('wall', -1, 1.02); }
        if (ballX + BALL_R > FRAME_W) { ballX = FRAME_W - BALL_R; ballVX = -Math.abs(ballVX); sfx('wall', 1, 1.02); }
        if (ballY - BALL_R < 0) { ballY = BALL_R; ballVY = Math.abs(ballVY); sfx('wall', 0, 1.05); }

        // paddle
        if (ballVY > 0 && ballY + BALL_R >= PADDLE_Y && ballY - BALL_R <= PADDLE_Y + PADDLE_H &&
            ballX + BALL_R >= paddleX && ballX - BALL_R <= paddleX + PADDLE_W) {
            ballY = PADDLE_Y - BALL_R;
            const rel = clamp((ballX - (paddleX + PADDLE_W / 2)) / (PADDLE_W / 2), -1, 1);
            const angle = rel * (60 * Math.PI / 180);
            ballSpeed = Math.min(ballSpeed + BALL_SPEEDUP, BALL_SPEED_MAX);
            ballVX = Math.sin(angle) * ballSpeed;
            ballVY = -Math.cos(angle) * ballSpeed;
            addShake(3);
            sfx('paddle', rel, 0.97 + Math.random() * 0.06);
        }

        // bricks
        for (const b of bricks) {
            if (b.hp <= 0) { continue; }
            if (ballX + BALL_R < b.x || ballX - BALL_R > b.x + BRICK_W ||
                ballY + BALL_R < b.y || ballY - BALL_R > b.y + BRICK_H) { continue; }
            // resolve along the shallower penetration axis
            const ox = Math.min(ballX + BALL_R - b.x, b.x + BRICK_W - (ballX - BALL_R));
            const oy = Math.min(ballY + BALL_R - b.y, b.y + BRICK_H - (ballY - BALL_R));
            if (ox < oy) {
                ballVX = -ballVX;
                ballX += (ballX < b.x + BRICK_W / 2 ? -ox : ox);
            } else {
                ballVY = -ballVY;
                ballY += (ballY < b.y + BRICK_H / 2 ? -oy : oy);
            }
            ballSpeed = Math.min(ballSpeed + 3, BALL_SPEED_MAX);
            hitBrick(b);
            break;
        }

        if (ballY - BALL_R > FRAME_H) { loseLife(); return; }
    }
}

function loseLife() {
    if (state === 'attract') {
        resetBall();
        return;
    }
    lives--;
    addShake(12);
    sfx('life', 0, 1);
    if (lives <= 0) {
        state = 'over';
        return;
    }
    resetBall();
    state = 'serve';
    timer = SERVE_DELAY;
}

function clearLayout() {
    if (layout >= LAYOUTS.length - 1) {
        state = 'win';
        return;
    }
    layout++;
    loadLayout(layout);
    ballSpeed = Math.min(ballSpeed + 40, BALL_SPEED_MAX);
    resetBall();
    state = 'serve';
    timer = SERVE_DELAY;
}

// ---- update --------------------------------------------------------------
function update(dt) {
    if (dt > 0.05) { dt = 0.05; }
    clock += dt;
    if (shake > 0) {
        shake = Math.max(0, shake - dt * 26);
        shakeX = (Math.random() * 2 - 1) * shake;
        shakeY = (Math.random() * 2 - 1) * shake;
    } else {
        shakeX = 0;
        shakeY = 0;
    }

    const attract = state === 'attract';
    if (attract) {
        // AI paddle tracks the ball with a lag
        const target = ballX - PADDLE_W / 2;
        paddleX += clamp(target - paddleX, -PADDLE_SPEED * dt, PADDLE_SPEED * dt);
        paddleX = clamp(paddleX, 0, FRAME_W - PADDLE_W);
        if (ballVX === 0 && ballVY === 0) { resetBall(); launchBall(); state = 'rally'; }
    } else if (state === 'serve' || state === 'rally' || state === 'life') {
        paddleInput(dt);
    }

    if (state === 'serve') {
        resetBall();
        timer -= dt;
        if (timer <= 0) { launchBall(); }
    } else if (state === 'rally' || attract) {
        moveBall(dt);
        if (state === 'rally' && !bricksLeft()) {
            if (attract) {
                loadLayout(0);
                resetBall();
                launchBall();
            } else {
                clearLayout();
            }
        }
    }
}

// ---- render --------------------------------------------------------------
function bar(x, y, w, h, c) {
    efx.graphics.drawQuad(efx.graphics.whiteTexture, x + shakeX, y + shakeY, {
        size: [w, h],
        origin: [0, 0],
        color: c,
    });
}

function render() {
    efx.graphics.setBlendMode('alpha');

    for (const b of bricks) {
        if (b.hp <= 0) { continue; }
        const base = HP_COLOR[b.maxHp] || [1, 1, 1, 1];
        const dim = b.hp < b.maxHp ? 0.55 : 1;
        bar(b.x, b.y, BRICK_W, BRICK_H, [base[0] * dim, base[1] * dim, base[2] * dim, 1]);
        bar(b.x, b.y, BRICK_W, 2, [1, 1, 1, 0.6]);
    }

    efx.graphics.drawParticles(burst);

    bar(paddleX, PADDLE_Y, PADDLE_W, PADDLE_H, COL_PADDLE);
    bar(ballX - BALL_R, ballY - BALL_R, BALL_R * 2, BALL_R * 2, COL_BALL);

    efx.graphics.drawText('SCORE ' + score, hudFont, 16, 14, { color: COL_TEXT });
    efx.graphics.drawText('LIVES ' + Math.max(0, lives), hudFont, FRAME_W - 16, 14, {
        align: 'right', color: COL_TEXT,
    });

    if (state === 'attract') {
        efx.graphics.drawText('BLOOM BREAKOUT', bigFont, 320, 176, {
            align: 'center', color: [1, 1, 1, 0.92],
            outlineColor: [0.5, 0.2, 0.9, 1],
        });
        const a = 0.4 + 0.35 * Math.sin(clock * 3.0);
        efx.graphics.drawText('move / click to play', hudFont, 320, 244, {
            align: 'center', color: [0.7, 0.85, 1, a],
        });
    } else if (state === 'over') {
        efx.graphics.drawText('GAME OVER', bigFont, 320, 176, { align: 'center', color: [1, 0.4, 0.4, 1] });
        efx.graphics.drawText('press any key to play again', hudFont, 320, 244, {
            align: 'center', color: [0.8, 0.85, 1, 0.9],
        });
    } else if (state === 'win') {
        efx.graphics.drawText('YOU WIN', bigFont, 320, 176, { align: 'center', color: [1, 0.95, 0.5, 1] });
        efx.graphics.drawText('score ' + score + '  —  press any key to play again', hudFont, 320, 244, {
            align: 'center', color: [0.85, 0.9, 1, 0.9],
        });
    }
}
