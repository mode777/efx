// Mini Golf — rung 4 of the game gallery series. The first 3D rung and the
// first game built on impulse dynamics: nine data-authored holes, a dynamic
// ball tuned for roll, raycast cursor aiming onto the course plane, a
// hold-to-charge power meter, and a sensor cup. It self-plays until the first
// input.

efx.graphics.setClearColor([0.05, 0.08, 0.13, 1]);

const FRAME_W = 640;
const FRAME_H = 480;

const BALL_R = 0.35;
const IMPULSE_MAX = 5.6;
const CAPTURE_SPEED = 3.6;
const CUP_R = 0.55;
const CUP_CAP = 0.9; // capture radius (analytic fallback)

const CAM_OFFSET = [0, 7.5, 9.5];
const CAM_FOV = 48;

const HOLE_HALF_X = 7;
const HOLE_HALF_Z = 5;
const WALL_T = 0.4;
const WALL_H = 1;

// ---- fonts ---------------------------------------------------------------
const hudFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 22, {
    outline: { width: 1 },
    shadow: { blur: 3, offset: [1, 1] },
});
const bigFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 44, {
    outline: { width: 2 },
});

// ---- textures ------------------------------------------------------------
// grass is engine-generated noise (tinted green by the material below); the
// blob shadow is the shared pre-built radial image, tinted dark at draw time.
const grass = efx.graphics.createTexture(
    efx.noise.imageData(64, 64, { seed: 0x1234, type: 'value', frequency: 0.15, octaves: 2 }),
    { wrap: 'repeat' });
const shadowTex = efx.graphics.createTexture(efx.graphics.loadImage('radial.png'));

// ---- meshes --------------------------------------------------------------
const cube = efx.graphics.createMesh(efx.graphics.makeCube());
const ballMesh = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: BALL_R, segments: 20 }));
const groundMesh = efx.graphics.createMesh(efx.graphics.makePlane({ size: 40 }));

groundMesh.setSurfaceMaterial(0, {
    ambient: { color: [0.18, 0.22, 0.18, 1] },
    diffuse: { color: [0.36, 0.72, 0.4, 1], map: grass },
    specular: { color: [0.1, 0.12, 0.1, 1], shininess: 8 },
});
cube.setSurfaceMaterial(0, {
    ambient: { color: [0.16, 0.18, 0.24, 1] },
    diffuse: { color: [0.8, 0.85, 0.95, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 28 },
});
ballMesh.setSurfaceMaterial(0, {
    ambient: { color: [0.25, 0.25, 0.3, 1] },
    diffuse: { color: [1.0, 0.95, 0.6, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 64 },
});

// ---- course data ---------------------------------------------------------
// Each hole: tee/cup on the XZ plane, par, obstacle boxes, optional ramps
// (a ramp rises along +z from height 0 to h).
const HOLES = [
    { tee: [-5, 3], cup: [5, -3], par: 2, boxes: [] },
    { tee: [-5, 3], cup: [5, -3], par: 3, boxes: [{ x: 0, z: 0, w: 1.6, d: 3.4 }] },
    { tee: [-5, -3], cup: [5, 3], par: 3, boxes: [{ x: -1, z: 0, w: 1.2, d: 1.2 }, { x: 2.5, z: -1.5, w: 1.2, d: 1.2 }], ramps: [{ x: 3, z: 1.4, w: 3, d: 3, h: 0.5 }] },
    { tee: [-5, 3], cup: [5, 3], par: 2, boxes: [{ x: 0, z: 1, w: 4, d: 0.6 }, { x: 0, z: -1.5, w: 4, d: 0.6 }] },
    { tee: [0, 4], cup: [0, -4], par: 2, boxes: [{ x: -2, z: 0, w: 0.6, d: 3 }, { x: 2, z: 0, w: 0.6, d: 3 }] },
    { tee: [-6, 0], cup: [6, 0], par: 3, boxes: [{ x: -2, z: 1.2, w: 1.4, d: 1.4 }, { x: 1, z: -1.2, w: 1.4, d: 1.4 }, { x: 4, z: 1.2, w: 1.4, d: 1.4 }] },
    { tee: [-5, 3], cup: [5, -3], par: 3, boxes: [{ x: 1, z: 1.5, w: 2, d: 1 }], ramps: [{ x: 3, z: -0.5, w: 3, d: 3, h: 0.55 }] },
    { tee: [0, 4], cup: [0, -4], par: 3, boxes: [{ x: -1.5, z: 1.5, w: 1.2, d: 1.2 }, { x: 1.5, z: -1.5, w: 1.2, d: 1.2 }] },
    { tee: [-6, 3], cup: [6, -3], par: 3, boxes: [{ x: -2, z: 0, w: 1.2, d: 1.2 }, { x: 1, z: -2, w: 1.2, d: 1.2 }, { x: 3, z: 1.5, w: 1.2, d: 1.2 }], ramps: [{ x: 1.5, z: 2, w: 3, d: 3, h: 0.45 }] },
];

// ---- physics world -------------------------------------------------------
efx.physics.gravity = [0, -9.81, 0];
efx.physics.iterations = 8;
efx.physics.clear();

const ground = efx.physics.createBody({ type: 'box', size: [40, 1, 40] }, { position: [0, -0.5, 0], friction: 0.55 });
let ball = null; // (re)created at each hole's tee

let holeBodies = [];   // wall/obstacle/ramp bodies, rebuilt per hole
let holeRender = [];   // { mesh, transform, color } drawn each frame
let cupSensor = null;

function destroyHole() {
    for (const b of holeBodies) { b.destroy(); }
    for (const item of holeRender) {
        if (item.own && item.mesh) { item.mesh.destroy(); }
    }
    holeBodies = [];
    holeRender = [];
    if (cupSensor) { cupSensor.destroy(); cupSensor = null; }
}


function addBox(x, z, w, d, color) {
    const body = efx.physics.createBody({ type: 'box', size: [w, WALL_H, d] }, { position: [x, WALL_H / 2, z], friction: 0.5 });
    holeBodies.push(body);
    holeRender.push({ mesh: cube, transform: efx.math.mat4.transform([x, WALL_H / 2, z], 0, 0, 0, [w, WALL_H, d]), color: color || [0.45, 0.5, 0.62, 1] });
}

function addRamp(r) {
    const hw = r.w / 2;
    const hd = r.d / 2;
    const x = r.x;
    const z = r.z;
    const positions = [
        x - hw, 0, z - hd,
        x + hw, 0, z - hd,
        x + hw, r.h, z + hd,
        x - hw, r.h, z + hd,
    ];
    const nx = -r.h / Math.hypot(r.h, r.d);
    const ny = r.d / Math.hypot(r.h, r.d);
    const normals = [nx, ny, 0, nx, ny, 0, nx, ny, 0, nx, ny, 0];
    const indices = [0, 1, 2, 0, 2, 3];
    const mesh = efx.graphics.createMesh(efx.graphics.createMeshData([{ positions, normals, indices }]));
    mesh.setSurfaceMaterial(0, {
        ambient: { color: [0.18, 0.2, 0.26, 1] },
        diffuse: { color: [0.6, 0.65, 0.78, 1] },
        specular: { color: [1, 1, 1, 1], shininess: 24 },
    });
    const body = efx.physics.createStaticMesh(mesh, { friction: 0.6 });
    holeBodies.push(body);
    holeRender.push({ mesh: mesh, transform: efx.math.mat4.identity(), color: null, own: true });
}

function buildHole(index) {
    destroyHole();
    const h = HOLES[index];
    // perimeter walls
    addBox(0, HOLE_HALF_Z + WALL_T / 2, HOLE_HALF_X * 2 + WALL_T, WALL_T, [0.3, 0.34, 0.44, 1]);
    addBox(0, -(HOLE_HALF_Z + WALL_T / 2), HOLE_HALF_X * 2 + WALL_T, WALL_T, [0.3, 0.34, 0.44, 1]);
    addBox(-(HOLE_HALF_X + WALL_T / 2), 0, WALL_T, HOLE_HALF_Z * 2, [0.3, 0.34, 0.44, 1]);
    addBox(HOLE_HALF_X + WALL_T / 2, 0, WALL_T, HOLE_HALF_Z * 2, [0.3, 0.34, 0.44, 1]);
    // obstacles
    for (const b of (h.boxes || [])) {
        addBox(b.x, b.z, b.w, b.d, [0.5, 0.55, 0.7, 1]);
    }
    for (const r of (h.ramps || [])) { addRamp(r); }
    // cup sensor
    cupSensor = efx.physics.createBody({ type: 'sphere', radius: CUP_R }, { sensor: true, position: [h.cup[0], BALL_R, h.cup[1]] });
    // teleport the ball to the tee (or create it on the first hole)
    const tee = [h.tee[0], BALL_R, h.tee[1]];
    if (ball) {
        ball.setPosition(tee);
    } else {
        ball = efx.physics.createBody({ type: 'sphere', radius: BALL_R }, {
            dynamic: true, mass: 1, friction: 0.35, restitution: 0.32, position: tee,
        });
    }
}

// ---- state ---------------------------------------------------------------
// states: 'attract' | 'aim' | 'charge' | 'roll' | 'sunk' | 'scorecard'
let attract = true;
let state = 'attract';
let hole = 0;
let strokes = 0;
let totalStrokes = 0;
let parTotal = 0;
let holeScores = [];
let stillTime = 0;
let sinkTimer = 0;
let chargeT = 0;
let aimDir = [0, -1];
let pointer = [FRAME_W / 2, FRAME_H * 0.4];
let havePointer = false;
let restartEdge = false;


function startHole(index) {
    hole = index;
    strokes = 0;
    buildHole(index);
    state = 'aim';
    aimDir = [0, -1];
    stillTime = 0;
}

function startRound() {
    holeScores = [];
    totalStrokes = 0;
    parTotal = 0;
    startHole(0);
}

function wake() {
    if (!attract) { return; }
    attract = false;
    startRound();
}

function fire(power) {
    const p = efx.math.clamp(power, 0.06, 1);
    ball.applyImpulse([aimDir[0] * p * IMPULSE_MAX, 0, aimDir[1] * p * IMPULSE_MAX]);
    strokes++;
    state = 'roll';
    stillTime = 0;
}

function nextHole() {
    holeScores.push(strokes);
    totalStrokes += strokes;
    parTotal += HOLES[hole].par;
    if (hole >= HOLES.length - 1) {
        state = 'scorecard';
        return;
    }
    startHole(hole + 1);
}

// ---- input ---------------------------------------------------------------
efx.keyboard.onDown(function () {
    restartEdge = true;
    if (state === 'attract') { wake(); }
    else if (state === 'scorecard') { startRound(); }
});
efx.mouse.onDown(function () {
    restartEdge = true;
    if (state === 'attract') { wake(); }
    else if (state === 'scorecard') { startRound(); }
});
efx.mouse.onMove(function (e) {
    // events arrive in frame pixels, the space this game draws in
    pointer = [e.x, e.y];
    havePointer = true;
    if (state === 'attract') { wake(); }
});

// ---- camera + aiming -----------------------------------------------------
// The camera follows the ball; set it in update (before picking) and again in
// render so the engine's screenRay sees the frame the player is looking at.
function applyCamera() {
    const b = ball.position;
    const pos = [b[0] + CAM_OFFSET[0], b[1] + CAM_OFFSET[1], b[2] + CAM_OFFSET[2]];
    efx.graphics.setCamera3D(pos, [b[0], b[1] + 0.2, b[2]], CAM_FOV);
}

function cursorRay() {
    // the engine's picking ray through the cursor (frame pixels)
    const ray = efx.graphics.screenRay(pointer[0], pointer[1]);
    return { c: ray.origin, d: ray.direction };
}

function groundHit() {
    const { c, d } = cursorRay();
    // prefer the engine query; fall back to an analytic plane hit at y = 0
    const hit = efx.physics.raycast(c, d, 80);
    if (hit && hit.point) { return hit.point; }
    if (d[1] >= -0.001) { return null; }
    const t = -c[1] / d[1];
    if (t <= 0 || t > 80) { return null; }
    return [c[0] + d[0] * t, 0, c[2] + d[2] * t];
}

function updateAim() {
    if (!havePointer) { return; }
    const p = groundHit();
    if (!p) { return; }
    const b = ball.position;
    const dx = p[0] - b[0];
    const dz = p[2] - b[2];
    const len = Math.hypot(dx, dz);
    if (len < 0.4) { return; }
    aimDir = [dx / len, dz / len];
}

// ---- simulation ----------------------------------------------------------
function speed() {
    const v = ball.velocity;
    return Math.hypot(v[0], v[1], v[2]);
}

function cupReached() {
    const b = ball.position;
    const h = HOLES[hole];
    const d = Math.hypot(b[0] - h.cup[0], b[2] - h.cup[1]);
    if (d > CUP_CAP) { return false; }
    // sensor query (shows the overlap surface); analytic distance is the
    // authoritative capture test
    const hits = efx.physics.overlap({ type: 'sphere', radius: CUP_R }, { position: [h.cup[0], BALL_R, h.cup[1]] });
    const inSensor = hits.indexOf(ball) >= 0 || d <= CUP_CAP;
    return inSensor && speed() < CAPTURE_SPEED;
}

function stepSim(dt) {
    efx.physics.step(dt);
    if (state === 'roll') {
        if (cupReached()) {
            ball.velocity = [0, 0, 0];
            state = 'sunk';
            sinkTimer = 1.1;
            return;
        }
        if (speed() < 0.14) {
            stillTime += dt;
            if (stillTime > 0.4) {
                ball.velocity = [0, 0, 0];
                if (strokes >= HOLES[hole].par * 2 + 2) {
                    nextHole();
                } else {
                    state = 'aim';
                }
            }
        } else {
            stillTime = 0;
        }
    }
}

// ---- update --------------------------------------------------------------
function update(dt) {
    if (dt > 0.05) { dt = 0.05; }
    clock = efx.time.now;
    applyCamera();

    if (attract) {
        // self-play: line the ball up at the cup with an error term
        if (state !== 'roll' && state !== 'sunk') {
            const b = ball.position;
            const h = HOLES[hole];
            let dx = h.cup[0] - b[0] + Math.sin(clock * 1.7) * 0.6;
            let dz = h.cup[1] - b[2] + Math.cos(clock * 2.1) * 0.6;
            const len = Math.hypot(dx, dz) || 1;
            aimDir = [dx / len, dz / len];
            chargeT += dt;
            if (chargeT > 0.6) { chargeT = 0; fire(0.72); }
        }
        stepSim(dt);
        if (state === 'sunk') {
            sinkTimer -= dt;
            if (sinkTimer <= 0) {
                startHole(hole >= HOLES.length - 1 ? 0 : hole + 1);
            }
        }
        return;
    }

    if (state === 'aim') {
        updateAim();
        if (efx.mouse.isDown('left')) {
            state = 'charge';
            chargeT = 0;
        }
    } else if (state === 'charge') {
        updateAim();
        chargeT += dt;
        if (!efx.mouse.isDown('left')) {
            fire(chargePower());
        }
    } else if (state === 'sunk') {
        sinkTimer -= dt;
        if (sinkTimer <= 0) { nextHole(); }
    } else if (state === 'scorecard') {
        if (restartEdge) { startRound(); }
    }

    stepSim(dt);
    restartEdge = false;
}

buildHole(0);
state = 'attract';

function chargePower() {
    // smooth ping-pong 0 -> 1 -> 0
    return 0.5 - 0.5 * Math.cos(chargeT * Math.PI * 1.8);
}

// ---- render --------------------------------------------------------------
function draw3d() {
    const b = ball.position;
    applyCamera();
    efx.graphics.setDirectionalLight({ dir: [-0.4, -1, -0.3], color: [0.55, 0.6, 0.7, 1] });
    efx.graphics.setLight(0, { pos: [b[0], 6, b[2] + 3], color: [1, 0.95, 0.8, 1], range: 40 });

    efx.graphics.drawMesh(groundMesh, { transform: efx.math.mat4.identity(), color: [1, 1, 1, 1] });

    for (const item of holeRender) {
        efx.graphics.drawMesh(item.mesh, { transform: item.transform, color: item.color });
    }

    // cup: a dark disc plus a flag pole
    const h = HOLES[hole];
    efx.graphics.drawBillboard(shadowTex, [h.cup[0], 0.02, h.cup[1]], {
        size: 1.5, facing: 'plane', normal: [0, 1, 0], color: [0.02, 0.02, 0.03, 0.95], blend: 'alpha',
    });
    efx.graphics.drawMesh(cube, { transform: efx.math.mat4.transform([h.cup[0], 0.7, h.cup[1]], 0, 0, 0, [0.05, 1.4, 0.05]), color: [0.9, 0.9, 0.95, 1] });

    // ball shadow
    efx.graphics.drawBillboard(shadowTex, [b[0], 0.02, b[2]], {
        size: 0.95, facing: 'plane', normal: [0, 1, 0], color: [0, 0, 0, 0.55], blend: 'alpha',
    });

    // aim line
    if (state === 'aim' || state === 'charge') {
        const len = 2.4;
        const mx = b[0] + aimDir[0] * len / 2;
        const mz = b[2] + aimDir[1] * len / 2;
        let m = efx.math.mat4.identity();
        m = efx.math.mat4.translate(m, [mx, 0.06, mz]);
        m = efx.math.mat4.rotate(m, Math.atan2(aimDir[0], aimDir[1]) * 180 / Math.PI, [0, 1, 0]);
        m = efx.math.mat4.scale(m, [0.06, 0.02, len]);
        efx.graphics.drawMesh(cube, { transform: m, color: [0.4, 1, 0.6, 1] });
    }

    efx.graphics.drawMesh(ballMesh, { transform: efx.math.mat4.transform(b, 0, 0, 0, [1, 1, 1]), color: [1, 1, 1, 1] });
}

function render() {
    draw3d();

    // 2D HUD
    efx.graphics.setCamera2D({ frame: [FRAME_W, FRAME_H] });
    efx.graphics.setBlendMode('alpha');
    const h = HOLES[hole];
    efx.graphics.drawText('HOLE ' + (hole + 1) + '/' + HOLES.length, hudFont, 16, 14, { color: [0.85, 0.9, 1, 1] });
    efx.graphics.drawText('PAR ' + h.par, hudFont, 16, 42, { color: [0.6, 0.75, 0.9, 1] });
    efx.graphics.drawText('STROKES ' + strokes, hudFont, FRAME_W - 16, 14, { align: 'right', color: [0.85, 0.9, 1, 1] });

    if (!attract && state === 'charge') {
        const p = chargePower();
        efx.graphics.drawQuad(efx.graphics.whiteTexture, 220, FRAME_H - 34, {
            size: [200, 12], origin: [0, 0], color: [0.12, 0.16, 0.22, 0.9],
        });
        efx.graphics.drawQuad(efx.graphics.whiteTexture, 220, FRAME_H - 34, {
            size: [200 * p, 12], origin: [0, 0], color: [0.4 + p * 0.6, 1 - p * 0.4, 0.4, 1],
        });
    } else if (!attract && state === 'aim') {
        efx.graphics.drawText('aim with the mouse — hold left button to charge, release to putt', hudFont, 320, FRAME_H - 30, {
            align: 'center', color: [0.7, 0.82, 1, 0.85],
        });
    }

    if (attract) {
        efx.graphics.drawText('MINI GOLF', bigFont, 320, 150, {
            align: 'center', color: [1, 1, 1, 0.92], outlineColor: [0.1, 0.5, 0.25, 1],
        });
        const a = 0.4 + 0.35 * Math.sin(clock * 3.0);
        efx.graphics.drawText('move / click to play', hudFont, 320, 214, {
            align: 'center', color: [0.75, 0.9, 1, a],
        });
    } else if (state === 'sunk') {
        efx.graphics.drawText(strokes === 1 ? 'HOLE IN ONE!' : 'IN THE CUP', bigFont, 320, 150, {
            align: 'center', color: [0.5, 1, 0.6, 1],
        });
    } else if (state === 'scorecard') {
        efx.graphics.drawText('SCORECARD', bigFont, 320, 70, { align: 'center', color: [1, 1, 1, 0.95] });
        efx.graphics.drawText('STROKES ' + totalStrokes + '   PAR ' + parTotal, hudFont, 320, 140, {
            align: 'center', color: [0.85, 0.9, 1, 1],
        });
        const diff = totalStrokes - parTotal;
        const label = diff === 0 ? 'EVEN PAR' : (diff > 0 ? '+' + diff : String(diff));
        efx.graphics.drawText(label, hudFont, 320, 172, {
            align: 'center', color: diff <= 0 ? [0.5, 1, 0.6, 1] : [1, 0.7, 0.5, 1],
        });
        efx.graphics.drawText('press any key to play again', hudFont, 320, 220, {
            align: 'center', color: [0.8, 0.85, 1, 0.9],
        });
    }
}
