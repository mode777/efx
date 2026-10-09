// Bot Arena — rung 6, the capstone of the game gallery series. A wave-based
// twin-stick arena composing 3D rendering, lighting, post bloom, particles,
// physics (character controller, dynamic bodies, impulses, raycasts), text,
// keyboard/mouse/gamepad input, and audio. The fixed 4-light budget is spent
// on muzzle flashes and explosions. It self-plays until the first input.

efx.graphics.setClearColor([0.02, 0.02, 0.05, 1]);
efx.graphics.setPostEffects([
    { effect: 'bloom', threshold: 0.6, strength: 0.9 },
    { effect: 'colorFilter', saturation: 1.1, contrast: 1.06 },
]);

const FRAME_W = 640;
const FRAME_H = 480;

const ARENA_HALF = 18;
const CAM_OFF = [0, 20, 15];
const CAM_FOV = 55;

const PLAYER_R = 0.4;
const PLAYER_H = 1.6;
const PLAYER_SPEED = 9.5;
const PLAYER_ACCEL = 70;
const PLAYER_HP = 100;
const FIRE_CD = 0.14;

const ENEMY_R = 0.6;
const ENEMY_ACCEL = 16;
const ENEMY_MAX = 5.0;
const ENEMY_HP = 2;
const SHOOTER_STANDOFF = 9;
const SHOOTER_CD = 1.15;

const BOLT_P_SPEED = 28;
const BOLT_E_SPEED = 15;
const BOLT_LIFE = 2.0;
const BOLT_HIT = 0.55;

const WAVE_INTERMISSION = 3.0;
const MAXP = 72;

const COL_PLAYER = [0.4, 0.9, 1.0, 1];
const COL_CHASER = [1.0, 0.35, 0.4, 1];
const COL_SHOOTER = [1.0, 0.7, 0.25, 1];
const COL_TEXT = [0.85, 0.9, 1.0, 1];

// ---- fonts ---------------------------------------------------------------
const hudFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 22, {
    outline: { width: 1 }, shadow: { blur: 3, offset: [1, 1] },
});
const bigFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 44, { outline: { width: 2 } });

// ---- procedural textures -------------------------------------------------
function gridTexture(size) {
    const px = [];
    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const gx = x % 16 === 0;
            const gy = y % 16 === 0;
            const base = 0.06 + ((x * 7 + y * 13) % 11) * 0.002;
            const v = (gx || gy) ? 0.16 : base;
            px.push(Math.round(v * 40), Math.round(v * 90), Math.round(v * 140), 255);
        }
    }
    return efx.graphics.createTexture(efx.graphics.createImageData(size, size, px), { wrap: 'repeat' });
}
function radialTexture(size) {
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
const grid = gridTexture(64);
const glowTex = radialTexture(48);
const shadowTex = radialTexture(48);

// ---- meshes --------------------------------------------------------------
const cube = efx.graphics.createMesh(efx.graphics.makeCube());
const sphere = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 1, segments: 16 }));
const capsule = efx.graphics.createMesh(efx.graphics.makeCapsule({ radius: PLAYER_R, height: PLAYER_H }));
const floorMesh = efx.graphics.createMesh(efx.graphics.makePlane({ size: 40 }));

floorMesh.setSurfaceMaterial(0, { ambient: { color: [0.1, 0.12, 0.18, 1] }, diffuse: { color: [1, 1, 1, 1], map: grid } });
cube.setSurfaceMaterial(0, { ambient: { color: [0.16, 0.18, 0.26, 1] }, diffuse: { color: [0.5, 0.55, 0.72, 1] }, specular: { color: [1, 1, 1, 1], shininess: 32 } });
sphere.setSurfaceMaterial(0, { ambient: { color: [0.2, 0.16, 0.2, 1] }, diffuse: { color: [1, 1, 1, 1] }, specular: { color: [1, 1, 1, 1], shininess: 40 } });
capsule.setSurfaceMaterial(0, { ambient: { color: [0.12, 0.2, 0.24, 1] }, diffuse: { color: [0.6, 0.95, 1, 1] }, specular: { color: [1, 1, 1, 1], shininess: 48 } });

// ---- particles -----------------------------------------------------------
const burst = efx.graphics.createParticleSystem(glowTex, 500, [0.3, 0.9], {
    space: 'world', facing: 'view', emissionRate: 0, position: [0, 1, 0],
    direction: [0, 1, 0], spread: 180, speed: [3, 11], gravity: [0, 4, 0],
    linearDamping: [1.5, 3.0], sizes: [0.55, 0.05],
    colors: [[1, 1, 1, 0.95], [1, 0.4, 0.1, 0]],
    blend: 'additive',
});
function burstAt(x, y, z, color, n) {
    burst.set({ position: [x, y, z], colors: [color, [color[0] * 0.6, color[1] * 0.2, color[2] * 0.1, 0]] });
    burst.emit(n || 22);
}

// ---- audio ---------------------------------------------------------------
const SFX = {
    shot: efx.audio.loadAudioData('shot.wav'),
    hit: efx.audio.loadAudioData('hit.wav'),
    explosion: efx.audio.loadAudioData('explosion.wav'),
    wave: efx.audio.loadAudioData('wave.wav'),
};
const musicStream = efx.audio.loadAudioStream('music.wav');
let music = null;
let musicVolume = 0.45;
let muted = false;
let audioReady = false;
function unlockAudio() {
    if (audioReady) { return; }
    audioReady = true;
    try { efx.audio.resume(); } catch (e) { /* no audio device */ }
    music = efx.audio.playAudio(musicStream, { loop: true, volume: muted ? 0 : musicVolume });
}
function sfx(name, pan, pitch) {
    if (!audioReady) { return; }
    efx.audio.playAudio(SFX[name], { volume: 0.8, pan: Math.max(-1, Math.min(1, pan || 0)), pitch: pitch || 1 });
}
function applyVolume() { if (music) { music.volume = muted ? 0 : musicVolume; } }

// ---- physics -------------------------------------------------------------
efx.physics.gravity = [0, 0, 0];
efx.physics.iterations = 8;
efx.physics.clear();

efx.physics.createBody({ type: 'box', size: [40, 1, 40] }, { position: [0, -0.5, 0] });
const walls = [];
for (const [x, z, w, d] of [
    [0, ARENA_HALF + 0.5, 40, 1], [0, -ARENA_HALF - 0.5, 40, 1],
    [ARENA_HALF + 0.5, 0, 1, 40], [-ARENA_HALF - 0.5, 0, 1, 40],
]) {
    walls.push({ body: efx.physics.createBody({ type: 'box', size: [w, 2, d] }, { position: [x, 1, z] }), x, z, w, d });
}
const PILLARS = [
    { x: -8, z: -8, w: 3, d: 3 }, { x: 8, z: -8, w: 3, d: 3 },
    { x: -8, z: 8, w: 3, d: 3 }, { x: 8, z: 8, w: 3, d: 3 },
    { x: 0, z: 0, w: 3, d: 3 },
];
for (const p of PILLARS) {
    efx.physics.createBody({ type: 'box', size: [p.w, 2, p.d] }, { position: [p.x, 1, p.z] });
}

let player = null;
function spawnPlayer(p) {
    if (player) { player.destroy(); }
    player = efx.physics.createCharacter(PLAYER_R, PLAYER_H, { position: p, up: [0, 1, 0] });
}
spawnPlayer([0, PLAYER_H / 2, 12]);

// ---- state ---------------------------------------------------------------
let attract = true;
let over = false;
let hp = PLAYER_HP;
let score = 0;
let wave = 0;
let waveState = 'intermission';
let interT = WAVE_INTERMISSION;
let spawnQueue = [];
let spawnT = 0;
let fireCd = 0;
let clock = 0;
let shake = 0;
let shakeX = 0;
let shakeY = 0;
let aimDir = [0, -1];
let pointer = [FRAME_W / 2, FRAME_H * 0.35];
let havePointer = false;
let restartEdge = false;
let vel = [0, 0];

const enemies = [];
const bolts = [];
for (let i = 0; i < MAXP; i++) { bolts.push({ active: false, owner: 0, x: 0, y: 0, z: 0, vx: 0, vz: 0, life: 0 }); }
const flashes = [];

function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
function clampLen(x, z, m) {
    const l = Math.hypot(x, z);
    if (l > m) { return [x / l * m, z / l * m]; }
    return [x, z];
}
function dist2(ax, az, bx, bz) { const dx = ax - bx; const dz = az - bz; return Math.sqrt(dx * dx + dz * dz); }

// ---- enemies / waves -----------------------------------------------------
function spawnEnemy(kind) {
    const a = Math.random() * Math.PI * 2;
    const r = 15;
    const x = Math.cos(a) * r;
    const z = Math.sin(a) * r;
    const body = efx.physics.createBody({ type: 'sphere', radius: ENEMY_R }, {
        dynamic: true, mass: 1, friction: 0.7, restitution: 0.2, position: [x, ENEMY_R, z],
    });
    enemies.push({ body, kind, flash: 0, fireCd: 0.6 + Math.random() * 0.6, hp: ENEMY_HP });
}

function beginIntermission() { waveState = 'intermission'; interT = WAVE_INTERMISSION; }
function beginWave() {
    wave++;
    spawnQueue = [];
    for (let i = 0; i < 2 + wave; i++) { spawnQueue.push('chaser'); }
    for (let i = 0; i < Math.max(0, wave - 1); i++) { spawnQueue.push('shooter'); }
    // shuffle deterministically enough
    for (let i = spawnQueue.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        const t = spawnQueue[i]; spawnQueue[i] = spawnQueue[j]; spawnQueue[j] = t;
    }
    waveState = 'active';
    spawnT = 0.3;
    sfx('wave', 0, 1);
}

function killEnemy(e, index) {
    const p = e.body.position;
    burstAt(p[0], p[1] + 0.3, p[2], [1, 0.6, 0.25, 1], 30);
    addFlash(p, [1, 0.6, 0.3, 1], 0.32, 2);
    sfx('explosion', clamp(p[0] / ARENA_HALF, -1, 1), 0.95 + Math.random() * 0.1);
    addShake(7);
    score += 100;
    e.body.destroy();
    enemies.splice(index, 1);
}

function enemyAI(e, dt) {
    const p = e.body.position;
    const pp = player.position;
    const dx = pp[0] - p[0];
    const dz = pp[2] - p[2];
    const d = Math.hypot(dx, dz) || 1;
    const nx = dx / d;
    const nz = dz / d;
    let fx = 0;
    let fz = 0;
    if (e.kind === 'chaser') {
        fx = nx * ENEMY_ACCEL;
        fz = nz * ENEMY_ACCEL;
    } else {
        // shooter: hold a stand-off ring and strafe
        if (d > SHOOTER_STANDOFF + 1) { fx = nx * ENEMY_ACCEL; fz = nz * ENEMY_ACCEL; }
        else if (d < SHOOTER_STANDOFF - 1) { fx = -nx * ENEMY_ACCEL; fz = -nz * ENEMY_ACCEL; }
        fx += -nz * ENEMY_ACCEL * 0.35;
        fz += nx * ENEMY_ACCEL * 0.35;
        e.fireCd -= dt;
        if (e.fireCd <= 0) {
            e.fireCd = SHOOTER_CD + Math.random() * 0.4;
            // line of sight against cover
            const origin = [p[0], 0.6, p[2]];
            const dir = [nx, 0, nz];
            const ray = efx.physics.raycast(origin, dir, d, {});
            const blocked = ray && ray.distance < d - 0.8;
            if (!blocked) { fireBolt(1, p[0] + nx, 0.6, p[2] + nz, nx, nz); }
        }
    }
    e.body.applyForce([fx, 0, fz]);
}

// ---- bolts ---------------------------------------------------------------
function freeBolt() {
    for (const b of bolts) { if (!b.active) { return b; } }
    return null;
}
function fireBolt(owner, x, y, z, nx, nz) {
    const b = freeBolt();
    if (!b) { return; }
    const sp = owner === 0 ? BOLT_P_SPEED : BOLT_E_SPEED;
    b.active = true;
    b.owner = owner;
    b.x = x; b.y = y; b.z = z;
    b.vx = nx * sp; b.vz = nz * sp;
    b.life = BOLT_LIFE;
    if (owner === 0) {
        addFlash([x, y, z], [0.5, 0.9, 1, 1], 0.09, 1);
        sfx('shot', clamp(x / ARENA_HALF, -1, 1), 0.97 + Math.random() * 0.06);
    }
}
function stepBolts(dt) {
    const pp = player.position;
    for (const b of bolts) {
        if (!b.active) { continue; }
        b.x += b.vx * dt;
        b.z += b.vz * dt;
        b.life -= dt;
        if (b.life <= 0 || Math.abs(b.x) > ARENA_HALF || Math.abs(b.z) > ARENA_HALF) {
            b.active = false;
            continue;
        }
        if (b.owner === 0) {
            for (let i = 0; i < enemies.length; i++) {
                const e = enemies[i];
                const p = e.body.position;
                if (dist2(b.x, b.z, p[0], p[2]) < BOLT_HIT + ENEMY_R) {
                    const nx = b.vx; const nz = b.vz;
                    const l = Math.hypot(nx, nz) || 1;
                    e.body.applyImpulse([nx / l * 3.5, 0, nz / l * 3.5]);
                    e.flash = 0.18;
                    e.hp--;
                    sfx('hit', clamp(b.x / ARENA_HALF, -1, 1), 1.0 + Math.random() * 0.1);
                    b.active = false;
                    if (e.hp <= 0) { killEnemy(e, i); }
                    break;
                }
            }
        } else {
            if (dist2(b.x, b.z, pp[0], pp[2]) < BOLT_HIT + PLAYER_R) {
                damage(12);
                b.active = false;
            }
        }
    }
}

// ---- lights --------------------------------------------------------------
function addFlash(pos, color, ttl, priority) {
    flashes.push({ pos: [pos[0], pos[1], pos[2]], color, ttl, max: ttl, priority });
}
function updateLights(dt) {
    for (let i = flashes.length - 1; i >= 0; i--) {
        flashes[i].ttl -= dt;
        if (flashes[i].ttl <= 0) { flashes.splice(i, 1); }
    }
    flashes.sort((a, b) => (b.priority - a.priority) || (b.ttl - a.ttl));
    for (let i = 0; i < 4; i++) {
        const f = flashes[i];
        if (f) {
            const k = f.ttl / f.max;
            efx.graphics.setLight(i, {
                pos: f.pos,
                color: [f.color[0] * k, f.color[1] * k, f.color[2] * k, 1],
                range: f.priority >= 2 ? 16 : 10,
            });
        } else {
            efx.graphics.setLight(i, null);
        }
    }
}

// ---- damage / game over --------------------------------------------------
let hurtCd = 0;
function damage(amount) {
    if (hurtCd > 0) { return; }
    hp -= amount;
    hurtCd = 0.35;
    addShake(5);
    if (hp <= 0) {
        hp = 0;
        over = true;
        burstAt(player.position[0], 0.8, player.position[2], [1, 0.5, 0.2, 1], 40);
        sfx('explosion', 0, 1);
    }
}
function addShake(a) { shake = Math.min(16, shake + a); }

// ---- input ---------------------------------------------------------------
efx.keyboard.onDown(function (e) {
    restartEdge = true;
    if (attract) { wake(); return; }
    if (over) { resetGame(); return; }
    if (e.key === 'm' && !e.repeat) { muted = !muted; applyVolume(); }
    else if (e.key === '[') { musicVolume = clamp(musicVolume - 0.1, 0, 1); applyVolume(); }
    else if (e.key === ']') { musicVolume = clamp(musicVolume + 0.1, 0, 1); applyVolume(); }
});
efx.mouse.onDown(function () {
    restartEdge = true;
    if (attract) { wake(); } else if (over) { resetGame(); }
});
efx.mouse.onMove(function (e) {
    const s = efx.window.size;
    const fw = s[0] > 0 ? s[0] : FRAME_W;
    const fh = s[1] > 0 ? s[1] : FRAME_H;
    pointer = [e.x * (FRAME_W / fw), e.y * (FRAME_H / fh)];
    havePointer = true;
    if (attract) { wake(); }
});
function pad() { return efx.gamepad.count > 0 ? efx.gamepad.get(0) : null; }

// ---- aiming --------------------------------------------------------------
function cameraBasis() {
    const pp = player.position;
    const c = [pp[0] + CAM_OFF[0], pp[1] + CAM_OFF[1], pp[2] + CAM_OFF[2]];
    const target = [pp[0], pp[1], pp[2]];
    const f = [target[0] - c[0], target[1] - c[1], target[2] - c[2]];
    const fl = Math.hypot(f[0], f[1], f[2]) || 1;
    const fwd = [f[0] / fl, f[1] / fl, f[2] / fl];
    const right = [fwd[2], 0, -fwd[0]];
    const rl = Math.hypot(right[0], right[2]) || 1;
    const r = [right[0] / rl, 0, right[2] / rl];
    const up = [r[1] * fwd[2] - r[2] * fwd[1], r[2] * fwd[0] - r[0] * fwd[2], r[0] * fwd[1] - r[1] * fwd[0]];
    return { c, fwd, r, up };
}
function updateAim() {
    const p = pad();
    if (p && (Math.abs(p.axis('rightX')) > 0.25 || Math.abs(p.axis('rightY')) > 0.25)) {
        const ax = p.axis('rightX');
        const ay = p.axis('rightY');
        const l = Math.hypot(ax, ay) || 1;
        aimDir = [ax / l, ay / l];
        return;
    }
    if (!havePointer) { return; }
    const { c, fwd, r, up } = cameraBasis();
    const nx = (pointer[0] / FRAME_W) * 2 - 1;
    const ny = 1 - (pointer[1] / FRAME_H) * 2;
    const aspect = FRAME_W / FRAME_H;
    const t = Math.tan((CAM_FOV * Math.PI / 180) / 2);
    const d = [
        fwd[0] + r[0] * nx * t * aspect + up[0] * ny * t,
        fwd[1] + r[1] * nx * t * aspect + up[1] * ny * t,
        fwd[2] + r[2] * nx * t * aspect + up[2] * ny * t,
    ];
    const dl = Math.hypot(d[0], d[1], d[2]) || 1;
    const dir = [d[0] / dl, d[1] / dl, d[2] / dl];
    const hit = efx.physics.raycast(c, dir, 80);
    let hx;
    let hz;
    if (hit && hit.point) { hx = hit.point[0]; hz = hit.point[2]; }
    else if (dir[1] < -0.001) { const tt = -c[1] / dir[1]; hx = c[0] + dir[0] * tt; hz = c[2] + dir[2] * tt; }
    else { return; }
    const pp = player.position;
    const dx = hx - pp[0];
    const dz = hz - pp[2];
    const l = Math.hypot(dx, dz);
    if (l > 0.3) { aimDir = [dx / l, dz / l]; }
}

// ---- player movement -----------------------------------------------------
function playerMove(dt) {
    let ix = 0;
    let iz = 0;
    if (efx.keyboard.isDown('a') || efx.keyboard.isDown('left')) { ix -= 1; }
    if (efx.keyboard.isDown('d') || efx.keyboard.isDown('right')) { ix += 1; }
    if (efx.keyboard.isDown('w') || efx.keyboard.isDown('up')) { iz -= 1; }
    if (efx.keyboard.isDown('s') || efx.keyboard.isDown('down')) { iz += 1; }
    const p = pad();
    if (p) { ix += p.axis('leftX'); iz += p.axis('leftY'); }
    const l = Math.hypot(ix, iz);
    if (l > 1) { ix /= l; iz /= l; }
    const tx = ix * PLAYER_SPEED;
    const tz = iz * PLAYER_SPEED;
    vel[0] += clamp(tx - vel[0], -PLAYER_ACCEL * dt, PLAYER_ACCEL * dt);
    vel[1] += clamp(tz - vel[1], -PLAYER_ACCEL * dt, PLAYER_ACCEL * dt);
    player.moveAndSlide([vel[0] * dt, 0, vel[1] * dt]);
    player.velocity = [vel[0], 0, vel[1]];
}
function attractMove(dt) {
    // slow patrol; aim/fire at the nearest enemy
    const a = clock * 0.6;
    const tx = Math.cos(a) * 6;
    const tz = Math.sin(a * 1.3) * 6;
    const pp = player.position;
    const dx = tx - pp[0];
    const dz = tz - pp[2];
    const l = Math.hypot(dx, dz) || 1;
    vel[0] += clamp((dx / l) * PLAYER_SPEED * 0.6 - vel[0], -PLAYER_ACCEL * dt, PLAYER_ACCEL * dt);
    vel[1] += clamp((dz / l) * PLAYER_SPEED * 0.6 - vel[1], -PLAYER_ACCEL * dt, PLAYER_ACCEL * dt);
    player.moveAndSlide([vel[0] * dt, 0, vel[1] * dt]);
    player.velocity = [vel[0], 0, vel[1]];
    let best = null;
    let bd = 1e9;
    for (const e of enemies) {
        const p = e.body.position;
        const d = dist2(pp[0], pp[2], p[0], p[2]);
        if (d < bd) { bd = d; best = e; }
    }
    if (best) {
        const p = best.body.position;
        const ax = p[0] - pp[0];
        const az = p[2] - pp[2];
        const al = Math.hypot(ax, az) || 1;
        aimDir = [ax / al, az / al];
    }
}

// ---- lifecycle -----------------------------------------------------------
function resetGame() {
    for (const e of enemies) { e.body.destroy(); }
    enemies.length = 0;
    for (const b of bolts) { b.active = false; }
    hp = PLAYER_HP;
    score = 0;
    wave = 0;
    vel = [0, 0];
    over = false;
    hurtCd = 0;
    spawnPlayer([0, PLAYER_H / 2, 12]);
    beginIntermission();
}
function wake() {
    if (!attract) { return; }
    attract = false;
    unlockAudio();
    resetGame();
}

// ---- update --------------------------------------------------------------
function update(dt) {
    if (dt > 0.05) { dt = 0.05; }
    clock += dt;
    hurtCd = Math.max(0, hurtCd - dt);
    if (shake > 0) {
        shake = Math.max(0, shake - dt * 30);
        shakeX = (Math.random() * 2 - 1) * shake;
        shakeY = (Math.random() * 2 - 1) * shake;
    } else { shakeX = 0; shakeY = 0; }

    updateAim();

    if (attract) {
        attractMove(dt);
        fireCd -= dt;
        if (fireCd <= 0 && enemies.length) { fireCd = FIRE_CD; firePlayer(); }
    } else if (!over) {
        playerMove(dt);
        fireCd -= dt;
        const p = pad();
        const firing = efx.mouse.isDown('left') || (p && p.axis('rightTrigger') > 0.5) || efx.keyboard.isDown('space');
        if (firing && fireCd <= 0) { fireCd = FIRE_CD; firePlayer(); }
    }

    for (const e of enemies) { enemyAI(e, dt); }
    efx.physics.step(dt);
    // clamp enemy speed
    for (const e of enemies) {
        const v = e.body.velocity;
        const c = clampLen(v[0], v[2], ENEMY_MAX);
        e.body.velocity = [c[0], 0, c[1]];
        e.flash = Math.max(0, e.flash - dt);
    }

    stepBolts(dt);

    // enemy contact damage
    if (!attract && !over) {
        const pp = player.position;
        for (const e of enemies) {
            const p = e.body.position;
            if (dist2(pp[0], pp[2], p[0], p[2]) < PLAYER_R + ENEMY_R + 0.15) { damage(9); break; }
        }
    }

    // waves
    if (waveState === 'intermission') {
        interT -= dt;
        if (interT <= 0) { beginWave(); }
    } else {
        spawnT -= dt;
        if (spawnT <= 0 && spawnQueue.length) { spawnEnemy(spawnQueue.pop()); spawnT = 0.35; }
        if (!spawnQueue.length && enemies.length === 0) { beginIntermission(); }
    }

    updateLights(dt);
    restartEdge = false;
}

function firePlayer() {
    const pp = player.position;
    fireBolt(0, pp[0] + aimDir[0] * 0.8, 0.7, pp[2] + aimDir[1] * 0.8, aimDir[0], aimDir[1]);
}

// ---- render --------------------------------------------------------------
function trs(pos, scale) {
    const t = efx.math.mat4.translate(efx.math.mat4.identity(), pos);
    return efx.math.mat4.scale(t, scale);
}
function draw3d() {
    const pp = player.position;
    const camPos = [pp[0] + CAM_OFF[0] + shakeX * 0.02, CAM_OFF[1], pp[2] + CAM_OFF[2] + shakeY * 0.02];
    efx.graphics.setCamera3D(camPos, [pp[0], 0.5, pp[2]], CAM_FOV, { near: 0.1, far: 200 });
    efx.graphics.setDirectionalLight({ dir: [-0.3, -1, -0.2], color: [0.35, 0.4, 0.55, 1] });

    efx.graphics.drawMesh(floorMesh, { transform: efx.math.mat4.identity(), color: [1, 1, 1, 1] });
    for (const w of walls) {
        efx.graphics.drawMesh(cube, { transform: trs([w.x, 1, w.z], [w.w, 2, w.d]), color: [0.25, 0.3, 0.45, 1] });
    }
    for (const p of PILLARS) {
        efx.graphics.drawMesh(cube, { transform: trs([p.x, 1, p.z], [p.w, 2, p.d]), color: [0.3, 0.35, 0.5, 1] });
    }

    for (const e of enemies) {
        const p = e.body.position;
        const base = e.kind === 'chaser' ? COL_CHASER : COL_SHOOTER;
        const c = e.flash > 0 ? [1, 1, 1, 1] : base;
        efx.graphics.drawMesh(sphere, { transform: trs([p[0], ENEMY_R, p[2]], [ENEMY_R, ENEMY_R, ENEMY_R]), color: c });
        efx.graphics.drawBillboard(shadowTex, [p[0], 0.02, p[2]], {
            size: ENEMY_R * 2.2, facing: 'plane', normal: [0, 1, 0], color: [0, 0, 0, 0.45], blend: 'alpha',
        });
    }

    // player
    efx.graphics.drawMesh(capsule, { transform: trs([pp[0], PLAYER_H / 2, pp[2]], [1, 1, 1]), color: [1, 1, 1, 1] });
    efx.graphics.drawBillboard(shadowTex, [pp[0], 0.02, pp[2]], {
        size: PLAYER_R * 2.6, facing: 'plane', normal: [0, 1, 0], color: [0, 0, 0, 0.5], blend: 'alpha',
    });
    // aim marker
    efx.graphics.drawBillboard(glowTex, [pp[0] + aimDir[0] * 1.6, 0.1, pp[2] + aimDir[1] * 1.6], {
        size: 0.5, facing: 'plane', normal: [0, 1, 0], color: [0.5, 1, 0.7, 0.8], blend: 'additive',
    });

    for (const b of bolts) {
        if (!b.active) { continue; }
        const col = b.owner === 0 ? [0.5, 0.95, 1, 1] : [1, 0.5, 0.3, 1];
        let m = efx.math.mat4.identity();
        m = efx.math.mat4.translate(m, [b.x, b.y, b.z]);
        m = efx.math.mat4.rotate(m, Math.atan2(b.vx, b.vz) * 180 / Math.PI, [0, 1, 0]);
        m = efx.math.mat4.scale(m, [0.16, 0.16, 0.8]);
        efx.graphics.drawMesh(cube, { transform: m, color: col });
    }

    efx.graphics.drawParticles(burst);
}

function render() {
    draw3d();

    efx.graphics.setCamera2D({ frame: [FRAME_W, FRAME_H] });
    efx.graphics.setBlendMode('alpha');

    // health bar
    efx.graphics.drawQuad(efx.graphics.whiteTexture, 16, 16, { size: [180, 16], origin: [0, 0], color: [0.1, 0.12, 0.18, 0.9] });
    efx.graphics.drawQuad(efx.graphics.whiteTexture, 16, 16, {
        size: [180 * (hp / PLAYER_HP), 16], origin: [0, 0],
        color: hp > 40 ? [0.3, 0.95, 0.5, 1] : [1, 0.4, 0.4, 1],
    });
    efx.graphics.drawText('SCORE ' + score, hudFont, FRAME_W - 16, 14, { align: 'right', color: COL_TEXT });
    efx.graphics.drawText('WAVE ' + wave, hudFont, FRAME_W - 16, 42, { align: 'right', color: [0.7, 0.8, 1, 1] });

    if (waveState === 'intermission' && !attract && !over) {
        efx.graphics.drawText('WAVE ' + (wave + 1) + ' INCOMING', bigFont, 320, 150, {
            align: 'center', color: [1, 0.9, 0.6, 0.95],
        });
    }

    if (attract) {
        efx.graphics.drawText('BOT ARENA', bigFont, 320, 150, {
            align: 'center', color: [1, 1, 1, 0.92], outlineColor: [0.6, 0.2, 0.8, 1],
        });
        const a = 0.4 + 0.35 * Math.sin(clock * 3.0);
        efx.graphics.drawText('move / click to play', hudFont, 320, 214, {
            align: 'center', color: [0.75, 0.9, 1, a],
        });
    } else if (over) {
        efx.graphics.drawText('GAME OVER', bigFont, 320, 150, { align: 'center', color: [1, 0.4, 0.4, 1] });
        efx.graphics.drawText('score ' + score + '  wave ' + wave, hudFont, 320, 210, { align: 'center', color: COL_TEXT });
        efx.graphics.drawText('press any key to play again', hudFont, 320, 242, {
            align: 'center', color: [0.85, 0.9, 1, 0.9],
        });
    } else {
        efx.graphics.drawText('WASD move   mouse aim   click fire   M mute', hudFont, 320, FRAME_H - 30, {
            align: 'center', color: [0.6, 0.72, 0.9, 0.85],
        });
    }
}
