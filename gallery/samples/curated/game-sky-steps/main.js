// Sky Steps — rung 5 of the game gallery series. The character controller put
// to its intended use: a third-person collect-them-all platformer over a
// floating course. moveAndSlide handles steps/walls/floors; sensors are stars,
// checkpoints, and the finish; a CC0 rigged fox is drawn skinned and posed by
// speed. A CC0 sky dome and a blob shadow ground the scene. It self-plays
// until the first input.

efx.graphics.setClearColor([0.05, 0.07, 0.12, 1]);

const FRAME_W = 640;
const FRAME_H = 480;

const MOVE_SPEED = 6.5;
const ACCEL = 40;
const GRAVITY = 20;
const JUMP_V = 8.2;
const COYOTE = 0.12;
const JUMP_BUFFER = 0.14;

const KILL_Y = -16;

const CAM_DIST = 7.5;
const CAM_HEIGHT = 3.2;
const CAM_FOV = 52;

const FOX_SCALE = 100; // the fox mesh is authored under a 100x Z-up node (fox-walk recipe)
const FOX_Y_OFFSET = -0.9;

// ---- fonts ---------------------------------------------------------------
const hudFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 22, {
    outline: { width: 1 },
    shadow: { blur: 3, offset: [1, 1] },
});
const bigFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 44, {
    outline: { width: 2 },
});

// ---- procedural textures -------------------------------------------------
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
const shadowTex = radialTexture(48);

// ---- sky + meshes --------------------------------------------------------
const skyTex = efx.graphics.createTexture(efx.graphics.loadImage('sky.jpg'), { wrap: 'clamp' });
const sky = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 120, segments: 32, inverted: true }));
sky.setSurfaceMaterial(0, { unlit: true, diffuse: { color: [1, 1, 1, 1], map: skyTex } });

const cube = efx.graphics.createMesh(efx.graphics.makeCube());
const starMesh = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 0.5, segments: 14 }));
const fox = efx.graphics.createMesh(efx.graphics.loadMeshData('Fox.glb'));

cube.setSurfaceMaterial(0, {
    ambient: { color: [0.18, 0.2, 0.26, 1] },
    diffuse: { color: [0.6, 0.68, 0.85, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 24 },
});
starMesh.setSurfaceMaterial(0, {
    emissive: { color: [1.0, 0.85, 0.2, 1] },
    diffuse: { color: [1, 0.95, 0.6, 1] },
});

// ---- course data ---------------------------------------------------------
// Platforms are given by their top-surface y; the collider is centered below.
const PLATFORMS = [
    { x: 0, top: 0, w: 6, d: 6, start: true },
    { x: 7, top: 0.5, w: 3, d: 3, star: true },
    { x: 12, top: 1.2, w: 3, d: 3 },
    { x: 16, top: 2.0, w: 3, d: 3, star: true, checkpoint: true },
    { x: 21, top: 2.6, w: 3, d: 3, star: true },
    { x: 26, top: 3.2, w: 3, d: 3 },
    { x: 31, top: 3.8, w: 3, d: 3, star: true, checkpoint: true },
    { x: 36, top: 4.4, w: 3, d: 3, star: true },
    { x: 41, top: 5.0, w: 4, d: 4, star: true },
    { x: 47, top: 5.4, w: 6, d: 6, finish: true },
];
const PLATFORM_H = 1;

const STARS = [];
const CHECKPOINTS = [];
let FINISH = null;

for (const p of PLATFORMS) {
    const y = p.top - PLATFORM_H / 2;
    efx.physics.createBody({ type: 'box', size: [p.w, PLATFORM_H, p.d] }, { position: [p.x, y, 0], friction: 0.8 });
    if (p.star) {
        const b = efx.physics.createBody({ type: 'sphere', radius: 1.2 }, { sensor: true, position: [p.x, p.top + 1.1, 0] });
        STARS.push({ body: b, x: p.x, y: p.top + 1.1, z: 0, taken: false });
    }
    if (p.checkpoint) {
        const b = efx.physics.createBody({ type: 'box', size: [2.2, 2.6, 2.2] }, { sensor: true, position: [p.x, p.top + 1.3, 0] });
        CHECKPOINTS.push({ body: b, x: p.x, y: p.top + 1, z: 0 });
    }
    if (p.finish) {
        const b = efx.physics.createBody({ type: 'box', size: [2.4, 3.2, 2.4] }, { sensor: true, position: [p.x, p.top + 1.6, 0] });
        FINISH = { body: b, x: p.x, y: p.top, z: 0 };
    }
}

const start = PLATFORMS[0];
let hero = null;
function spawnHero(p) {
    if (hero) { hero.destroy(); }
    hero = efx.physics.createCharacter(0.4, 1.8, {
        position: p,
        floorMaxAngle: 55,
        stepHeight: 0.4,
        floorSnapLength: 0.25,
    });
}
spawnHero([start.x, start.top + 1, 0]);

// ---- state ---------------------------------------------------------------
let attract = true;
let state = 'play'; // 'play' | 'win'
let vel = [0, 0, 0];
let camYaw = 0;
let camPitch = 0.42;
let dragging = false;
let lastMouse = [0, 0];
let sinceFloor = 99;
let sinceJump = 99;
let clock = 0;
let elapsed = 0;
let starsTaken = 0;
let respawn = [start.x, start.top + 1, 0];
let restartEdge = false;
let heading = 0;

function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }

function resetRun() {
    for (const s of STARS) { s.taken = false; }
    starsTaken = 0;
    respawn = [start.x, start.top + 1, 0];
    placeHero(respawn);
    vel = [0, 0, 0];
    camYaw = 0;
    elapsed = 0;
    state = 'play';
}

function placeHero(p) {
    // a character's position is read-only: recreate it at the target
    spawnHero(p);
    vel = [0, 0, 0];
}

function wake() {
    if (!attract) { return; }
    attract = false;
    resetRun();
}

// ---- input ---------------------------------------------------------------
efx.keyboard.onDown(function (e) {
    restartEdge = true;
    if (attract) { wake(); return; }
    if (state === 'win') { resetRun(); return; }
    if (e.key === 'space' && !e.repeat) { sinceJump = 0; }
});
efx.mouse.onDown(function (e) {
    restartEdge = true;
    if (attract) { wake(); return; }
    if (state === 'win') { resetRun(); return; }
    dragging = true;
    lastMouse = [e.x, e.y];
});
efx.mouse.onUp(function () { dragging = false; });
efx.mouse.onMove(function (e) {
    if (attract) { wake(); }
    if (dragging) {
        camYaw -= (e.x - lastMouse[0]) * 0.006;
        camPitch = clamp(camPitch + (e.y - lastMouse[1]) * 0.004, 0.12, 1.05);
        lastMouse = [e.x, e.y];
    }
});

function pad() {
    return efx.gamepad.count > 0 ? efx.gamepad.get(0) : null;
}

// ---- simulation ----------------------------------------------------------
function inputAxes() {
    let ix = 0;
    let iz = 0;
    if (efx.keyboard.isDown('a') || efx.keyboard.isDown('left')) { ix -= 1; }
    if (efx.keyboard.isDown('d') || efx.keyboard.isDown('right')) { ix += 1; }
    if (efx.keyboard.isDown('w') || efx.keyboard.isDown('up')) { iz += 1; }
    if (efx.keyboard.isDown('s') || efx.keyboard.isDown('down')) { iz -= 1; }
    const p = pad();
    if (p) {
        ix += p.axis('leftX');
        iz -= p.axis('leftY');
    }
    const len = Math.hypot(ix, iz);
    if (len > 1) { ix /= len; iz /= len; }
    return [ix, iz];
}

function cameraForward() {
    return [-Math.sin(camYaw), 0, -Math.cos(camYaw)];
}

function stepHero(dt) {
    const fwd = cameraForward();
    const right = [-fwd[2], 0, fwd[0]];
    const [ix, iz] = inputAxes();
    const wishX = fwd[0] * iz + right[0] * ix;
    const wishZ = fwd[2] * iz + right[2] * ix;
    const targetX = wishX * MOVE_SPEED;
    const targetZ = wishZ * MOVE_SPEED;
    vel[0] += clamp(targetX - vel[0], -ACCEL * dt, ACCEL * dt);
    vel[2] += clamp(targetZ - vel[2], -ACCEL * dt, ACCEL * dt);

    if (pad() && pad().isPressed('south')) { sinceJump = 0; }
    sinceJump += dt;
    if (sinceFloor < COYOTE && sinceJump < JUMP_BUFFER && vel[1] <= 0.1) {
        vel[1] = JUMP_V;
        sinceFloor = 99;
        sinceJump = 99;
    }

    vel[1] -= GRAVITY * dt;
    vel[1] = Math.max(vel[1], -40);

    const move = hero.moveAndSlide([vel[0] * dt, vel[1] * dt, vel[2] * dt]);
    if (move.onFloor) {
        vel[1] = 0;
        sinceFloor = 0;
    } else {
        sinceFloor += dt;
    }
    hero.velocity = [vel[0], vel[1], vel[2]];
}

function checkSensors() {
    for (const s of STARS) {
        if (s.taken) { continue; }
        const hits = efx.physics.overlap({ type: 'sphere', radius: 1.2 }, { position: [s.x, s.y, s.z] });
        if (hits.indexOf(hero) >= 0) {
            s.taken = true;
            starsTaken++;
        }
    }
    for (const c of CHECKPOINTS) {
        const hits = efx.physics.overlap({ type: 'box', size: [2.2, 2.6, 2.2] }, { position: [c.x, c.y + 0.3, c.z] });
        if (hits.indexOf(hero) >= 0) {
            respawn = [c.x, c.y + 1, c.z];
        }
    }
    if (FINISH) {
        const hits = efx.physics.overlap({ type: 'box', size: [2.4, 3.2, 2.4] }, { position: [FINISH.x, FINISH.y + 1.6, FINISH.z] });
        if (hits.indexOf(hero) >= 0 && starsTaken >= STARS.length) {
            state = 'win';
        }
    }
}

function attractInput(dt) {
    // steer toward the nearest uncollected star, jump when close and grounded
    let target = null;
    let best = 1e9;
    for (const s of STARS) {
        if (s.taken) { continue; }
        const d = Math.hypot(s.x - hero.position[0], s.z - hero.position[2]);
        if (d < best) { best = d; target = s; }
    }
    let dx = 0;
    let dz = 0;
    if (target) {
        dx = target.x - hero.position[0];
        dz = target.z - hero.position[2];
        const l = Math.hypot(dx, dz) || 1;
        dx /= l; dz /= l;
        // ease the camera to look along the path
        camYaw += (Math.atan2(-dx, -dz) - camYaw) * Math.min(1, dt * 0.8);
    }
    const targetX = dx * MOVE_SPEED;
    const targetZ = dz * MOVE_SPEED;
    vel[0] += clamp(targetX - vel[0], -ACCEL * dt, ACCEL * dt);
    vel[2] += clamp(targetZ - vel[2], -ACCEL * dt, ACCEL * dt);
    if (hero.onFloor && target && best < 5) { vel[1] = JUMP_V; }
    vel[1] -= GRAVITY * dt;
    const move = hero.moveAndSlide([vel[0] * dt, vel[1] * dt, vel[2] * dt]);
    if (move.onFloor) { vel[1] = 0; }
    hero.velocity = [vel[0], vel[1], vel[2]];
}

function respawnIfFallen() {
    if (hero.position[1] < KILL_Y) {
        placeHero(respawn);
        vel = [0, 0, 0];
    }
}

// ---- update --------------------------------------------------------------
function update(dt) {
    if (dt > 0.05) { dt = 0.05; }
    clock += dt;

    if (attract) {
        attractInput(dt);
        respawnIfFallen();
        efx.physics.step(dt);
        checkSensors();
        if (state === 'win') {
            // the demo keeps running: restart the course after a beat
            elapsed = 0;
            state = 'play';
            resetRun();
        }
        heading = Math.atan2(vel[0], vel[2]);
        return;
    }

    if (state === 'play') {
        elapsed += dt;
        stepHero(dt);
        respawnIfFallen();
        efx.physics.step(dt);
        checkSensors();
    } else if (state === 'win') {
        if (restartEdge) { resetRun(); }
    }
    heading = Math.atan2(vel[0], vel[2]);
    restartEdge = false;
}

// ---- render --------------------------------------------------------------
function foxTransform() {
    const p = hero.position;
    const base = efx.math.mat4.scale(
        efx.math.mat4.rotate(efx.math.mat4.identity(), -90, [1, 0, 0]),
        [FOX_SCALE, FOX_SCALE, FOX_SCALE]);
    const orient = efx.math.mat4.rotate(efx.math.mat4.identity(), heading * 180 / Math.PI, [0, 1, 0]);
    const t = efx.math.mat4.translate(efx.math.mat4.identity(), [p[0], p[1] + FOX_Y_OFFSET, p[2]]);
    return efx.math.mat4.multiply(t, efx.math.mat4.multiply(orient, base));
}

function poseFox() {
    const sp = Math.hypot(vel[0], vel[2]);
    let clip = 'AnimalArmature|Idle';
    if (!hero.onFloor) { clip = 'AnimalArmature|Gallop'; }
    else if (sp > 3.5) { clip = 'AnimalArmature|Gallop'; }
    else if (sp > 0.4) { clip = 'AnimalArmature|Walk'; }
    try {
        fox.pose({ clip: clip, time: clock });
    } catch (e) {
        try { fox.pose({ clip: 'AnimalArmature|Walk', time: clock }); } catch (e2) { /* no rig */ }
    }
}

function draw3d() {
    const p = hero.position;
    const fwd = cameraForward();
    const camPos = [
        p[0] + fwd[0] * CAM_DIST * Math.cos(camPitch),
        p[1] + CAM_HEIGHT + CAM_DIST * Math.sin(camPitch),
        p[2] + fwd[2] * CAM_DIST * Math.cos(camPitch),
    ];
    efx.graphics.setCamera3D(camPos, [p[0], p[1] + 0.8, p[2]], CAM_FOV, { near: 0.1, far: 400 });
    efx.graphics.setDirectionalLight({ dir: [-0.4, -1, -0.3], color: [0.75, 0.8, 0.9, 1] });
    efx.graphics.setLight(0, { pos: [p[0], p[1] + 5, p[2] + 3], color: [1, 0.95, 0.85, 1], range: 30 });

    // sky first, camera-locked, no depth write
    efx.graphics.drawMesh(sky, {
        transform: efx.math.mat4.translate(efx.math.mat4.identity(), camPos),
        depthWrite: false,
    });

    for (const pl of PLATFORMS) {
        efx.graphics.drawMesh(cube, {
            transform: efx.math.mat4.translate(efx.math.mat4.identity(), [pl.x, pl.top - PLATFORM_H / 2, 0]),
            color: pl.start ? [0.4, 0.7, 0.5, 1] : (pl.finish ? [0.85, 0.7, 0.3, 1] : [0.55, 0.6, 0.78, 1]),
        });
        efx.graphics.drawMesh(cube, {
            transform: efx.math.mat4.translate(
                efx.math.mat4.scale(efx.math.mat4.identity(), [pl.w, 0.05, pl.d]),
                [pl.x, pl.top + 0.02, 0]),
            color: [0.7, 0.8, 0.95, 1],
        });
    }

    for (const s of STARS) {
        if (s.taken) { continue; }
        const bob = Math.sin(clock * 2.2 + s.x) * 0.15;
        efx.graphics.drawMesh(starMesh, {
            transform: efx.math.mat4.translate(efx.math.mat4.identity(), [s.x, s.y + bob, s.z]),
        });
    }
    for (const c of CHECKPOINTS) {
        efx.graphics.drawMesh(cube, {
            transform: efx.math.mat4.translate(
                efx.math.mat4.scale(efx.math.mat4.identity(), [0.2, 2.4, 0.2]),
                [c.x, c.y + 1.2, c.z]),
            color: [0.3, 0.9, 0.5, 1],
        });
    }
    if (FINISH) {
        const locked = starsTaken < STARS.length;
        efx.graphics.drawMesh(cube, {
            transform: efx.math.mat4.translate(
                efx.math.mat4.scale(efx.math.mat4.identity(), [0.3, 4, 0.3]),
                [FINISH.x, FINISH.y + 2, FINISH.z]),
            color: locked ? [0.9, 0.4, 0.4, 1] : [0.4, 1, 0.5, 1],
        });
    }

    // blob shadow under the fox
    efx.graphics.drawBillboard(shadowTex, [p[0], p[1] - 0.85, p[2]], {
        size: 1.3, facing: 'plane', normal: [0, 1, 0], color: [0, 0, 0, 0.5], blend: 'alpha',
    });

    poseFox();
    efx.graphics.drawMesh(fox, { transform: foxTransform(), skinned: true });
}

function render() {
    draw3d();

    efx.graphics.setCamera2D({ frame: [FRAME_W, FRAME_H] });
    efx.graphics.setBlendMode('alpha');
    efx.graphics.drawText('TIME ' + elapsed.toFixed(1) + 's', hudFont, 16, 14, { color: [0.9, 0.95, 1, 1] });
    efx.graphics.drawText('STARS ' + starsTaken + '/' + STARS.length, hudFont, FRAME_W - 16, 14, {
        align: 'right', color: starsTaken >= STARS.length ? [0.5, 1, 0.6, 1] : [0.9, 0.85, 0.5, 1],
    });

    if (attract) {
        efx.graphics.drawText('SKY STEPS', bigFont, 320, 150, {
            align: 'center', color: [1, 1, 1, 0.92], outlineColor: [0.15, 0.4, 0.7, 1],
        });
        const a = 0.4 + 0.35 * Math.sin(clock * 3.0);
        efx.graphics.drawText('move / click to play', hudFont, 320, 214, {
            align: 'center', color: [0.75, 0.9, 1, a],
        });
    } else if (state === 'win') {
        efx.graphics.drawText('COURSE CLEAR', bigFont, 320, 150, { align: 'center', color: [0.6, 1, 0.6, 1] });
        efx.graphics.drawText('time ' + elapsed.toFixed(1) + 's  —  press any key to play again', hudFont, 320, 214, {
            align: 'center', color: [0.85, 0.9, 1, 0.9],
        });
    } else {
        efx.graphics.drawText('WASD move   SPACE jump   drag to orbit', hudFont, 320, FRAME_H - 30, {
            align: 'center', color: [0.6, 0.72, 0.9, 0.85],
        });
    }
}
