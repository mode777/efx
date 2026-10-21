// Startup Flight: a boot-screen flight straight ahead over a rolling sea of
// cumulus under a deep blue sky, the EFX logo floating above the deck.
// Everything is 2D and painter's-ordered: sky -> sun rays -> cloud billows
// sorted far-to-near in one sprite batch (perspective computed in JS from
// focal*x/z) -> lens flare -> logo effects -> intro fade, then a bloom pass.
// Every cloud is a lit, opaque billow baked at load: a union of sphere caps
// (cauliflower crest) shaded by the sun, so bright crests sit over the
// blue-grey shadowed undersides of the row behind - the logo is the only
// image asset.

const G = efx.graphics;

// ---- palette ------------------------------------------------------------
const SKY_TOP = [0.04, 0.29, 0.76];
const SKY_HORIZON = [0.36, 0.63, 0.96];
const VALLEY_TOP = [0.60, 0.68, 0.84];   // deck shadow just under the horizon
const VALLEY_DEEP = [0.45, 0.53, 0.70];  // deep gaps toward the frame bottom
const HAZE = [0.80, 0.88, 1.00];         // aerial haze at the horizon
const LIT = [1.00, 0.995, 0.975];        // sunlit crest, a touch warm
const SHADE = [0.44, 0.52, 0.70];        // self-shadowed billow, blue-grey

// ---- layout: derived from the live surface at boot -----------------------
const W = efx.window.width || 640;
const H = efx.window.height || 480;
const CX = W * 0.5;
const HY = H * 0.40;                                     // horizon line
const FOCAL = (H * 0.5) / Math.tan(30 * Math.PI / 180);  // 60 deg vertical fov

// ---- flight (world units; camera at the origin looking down +z) ----------
const EYE = 6;            // eye height above the mean crest of the deck
const SPEED = 26;         // units/s, straight ahead: no bank, no pitch
const Z_NEAR = 0.8;
const Z_FAR = 330;
const X_SPAN = Z_FAR * (CX / FOCAL) * 1.1;
const CLOUD_COUNT = 900;

// ---- sun (upper right, matches the baked billow lighting) ----------------
const SUN_X = W * 0.86;
const SUN_Y = -H * 0.02;
const LIGHT = norm3(0.40, -0.80, 0.45); // screen x right, y down, z to viewer

// ---- logo -----------------------------------------------------------------
const LOGO_W = W * 0.30;
const LOGO_H = (LOGO_W * 265) / 619; // efx_logo.png is 619x265
const LOGO_Y = H * 0.5;
const LOGO_BOB = H * 0.012;
const LOGO_PERIOD = 3.5;
const LOGO_IN = 0.25;     // pop-in start (s)
const FLASH_T = 0.95;     // arrival flash (s)
const SHINE_START = 1.8;
const SHINE_PERIOD = 4.0;

// ---- helpers ----------------------------------------------------------------
function makeRng(seed) {
    let s = seed >>> 0;
    return () => {
        s = (s * 1664525 + 1013904223) >>> 0;
        return s / 4294967296;
    };
}
const rand = makeRng(0x2f6e2b1); // fixed seed: deterministic flight

function clamp01(x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
function smoothstep(a, b, x) {
    const t = clamp01((x - a) / (b - a));
    return t * t * (3 - 2 * t);
}
function mix(a, b, t) { return a + (b - a) * t; }
function norm3(x, y, z) {
    const l = Math.hypot(x, y, z);
    return [x / l, y / l, z / l];
}
function elasticOut(t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return Math.pow(2, -10 * t) * Math.sin((t * 10 - 0.75) * (2 * Math.PI / 3)) + 1;
}

// white RGB, alpha from fn(u, v) in [0,1]^2 - glows, rays, rings, gradients
function bakeAlpha(w, h, fn, opts) {
    const px = new Uint8Array(w * h * 4);
    for (let y = 0; y < h; y++) {
        const v = (y + 0.5) / h;
        for (let x = 0; x < w; x++) {
            const i = (y * w + x) * 4;
            px[i] = px[i + 1] = px[i + 2] = 255;
            px[i + 3] = clamp01(fn((x + 0.5) / w, v)) * 255;
        }
    }
    return G.createTexture(G.createImageData(w, h, px), opts || { wrap: 'clamp' });
}

// ---- cloud billow atlas -----------------------------------------------------
// Each billow is a union of sphere caps: big crest spheres along a profile
// that peaks in the middle, small spheres budding off their upper rims
// (the cauliflower bumps), and wide low body spheres filling the base. The
// winning sphere's normal is lit by the sun; the base fades out so the
// next row's crest paints over it.
const ATLAS = 1024;
const atlasPx = new Uint8Array(ATLAS * ATLAS * 4);
for (let i = 0; i < atlasPx.length; i += 4) { // neutral fringe color, no dark halos
    atlasPx[i] = 196; atlasPx[i + 1] = 206; atlasPx[i + 2] = 226; atlasPx[i + 3] = 0;
}
const SLOTS = [];

function bakeBillow(ox, oy, w, h, seed, roundness) {
    const r = makeRng(seed);
    const n = w * h;
    const zb = new Float32Array(n).fill(-1e9);
    const nxb = new Float32Array(n);
    const nyb = new Float32Array(n);
    const nzb = new Float32Array(n);
    const wob = new Float32Array(n);
    const fine = new Float32Array(n);
    efx.noise.fill2(wob, w, h, { seed, frequency: 6 / h, octaves: 4 });
    efx.noise.fill2(fine, w, h, { seed: seed + 1, frequency: 22 / h, octaves: 3 });

    const spheres = [];
    // keep every sphere (plus its wobble) inside the slot: no clipped edges
    const MARGIN = 6;
    const add = (x, y, z, rad) => {
        const room = Math.min(x - MARGIN, w - MARGIN - x, y - MARGIN) / 1.25;
        if (room > 2) spheres.push({ x, y, z, r: Math.min(rad, room) });
    };
    // mound profile: peaked, tapering toward both ends
    const topAt = (u) => h * (0.05 + (0.42 + 0.12 * roundness)
        * Math.pow(Math.abs(2 * u - 1), 1.5));
    const nBig = Math.round((w / h) * 1.5) + 1;
    for (let i = 0; i < nBig; i++) {
        const u = 0.15 + 0.70 * (i + 0.2 + 0.6 * r()) / nBig;
        const edge = Math.abs(2 * u - 1);
        const rad = h * (0.20 + 0.12 * r()) * (1 - 0.45 * edge) * (1 + 0.25 * roundness);
        const cx = u * w;
        const cy = topAt(u) + rad + h * 0.12 * r() * (1 - roundness * 0.5);
        add(cx, cy, 0, rad);
        const buds = 1 + ((r() * 2.5) | 0);
        for (let k = 0; k < buds; k++) {
            const a = -Math.PI / 2 + (r() - 0.5) * 2.2;
            const dist = rad * (0.62 + 0.12 * r());
            const sr = rad * (0.40 + 0.18 * r());
            const surf = Math.sqrt(Math.max(0, rad * rad - dist * dist));
            add(cx + Math.cos(a) * dist, cy + Math.sin(a) * dist, surf - sr * 0.6, sr);
        }
    }
    const nBody = Math.round((w / h) * 1.2) + 1;
    for (let i = 0; i < nBody; i++) {
        const u = 0.24 + 0.52 * (i + 0.5) / nBody;
        const edge = Math.abs(2 * u - 1);
        add(u * w, h * (0.66 + 0.06 * r()), -h * 0.12, h * 0.30 * (1 - 0.35 * edge));
    }

    // smooth union (polynomial smooth-max) rounds the creases between
    // spheres into soft valleys instead of hard seams
    const K = h * 0.10;
    for (const s of spheres) {
        const reach = s.r * 1.25;
        const x0 = Math.max(0, Math.floor(s.x - reach));
        const x1 = Math.min(w - 1, Math.ceil(s.x + reach));
        const y0 = Math.max(0, Math.floor(s.y - reach));
        const y1 = Math.min(h - 1, Math.ceil(s.y + reach));
        const inv = 1 / s.r;
        for (let y = y0; y <= y1; y++) {
            const dy = (y - s.y) * inv;
            for (let x = x0; x <= x1; x++) {
                const i = y * w + x;
                const dx = (x - s.x) * inv;
                const k = 1 + 0.22 * wob[i]; // wobbly, fluffy rim
                const d2 = (dx * dx + dy * dy) / (k * k);
                if (d2 >= 1) continue;
                const nz = Math.sqrt(1 - d2);
                const z = s.z + nz * s.r;
                const old = zb[i];
                if (old < -1e8) {
                    zb[i] = z; nxb[i] = dx / k; nyb[i] = dy / k; nzb[i] = nz;
                    continue;
                }
                const t = clamp01(0.5 + 0.5 * (z - old) / K);
                if (t <= 0) continue;
                zb[i] = old + (z - old) * t + K * t * (1 - t);
                nxb[i] += (dx / k - nxb[i]) * t;
                nyb[i] += (dy / k - nyb[i]) * t;
                nzb[i] += (nz - nzb[i]) * t;
            }
        }
    }

    const [lx, ly, lz] = LIGHT;
    for (let y = 0; y < h; y++) {
        const v = y / (h - 1);
        const baseFade = smoothstep(1.0, 0.78, v);
        // the lower body sits in the shadow of the crest above it
        const vertical = 1 - smoothstep(0.04, 0.92, v);
        for (let x = 0; x < w; x++) {
            const i = y * w + x;
            if (zb[i] < -1e8) continue;
            const nx = nxb[i];
            const ny = nyb[i];
            const nz = nzb[i];
            const il = 1 / (Math.hypot(nx, ny, nz) || 1);
            const lam = clamp01((nx * lx + ny * ly + nz * lz) * il * 0.75 + 0.30);
            // thin silhouettes scatter light: a silver rim instead of a dark fringe
            const rim = (1 - nz) * (1 - nz) * (1 - nz) * 0.45;
            const t = Math.pow(clamp01(0.5 * lam + 0.5 * vertical + rim + 0.03 * fine[i]), 1.25);
            const o = ((oy + y) * ATLAS + ox + x) * 4;
            atlasPx[o] = mix(SHADE[0], LIT[0], t) * 255;
            atlasPx[o + 1] = mix(SHADE[1], LIT[1], t) * 255;
            atlasPx[o + 2] = mix(SHADE[2], LIT[2], t) * 255;
            const edge = smoothstep(0.0, 0.5, nz + 0.10 * fine[i]);
            atlasPx[o + 3] = clamp01(edge * baseFade) * 255;
        }
    }
    // 4px transparent gutters keep mip levels from bleeding between slots
    SLOTS.push({ rect: { x: ox + 4, y: oy + 4, w: w - 8, h: h - 8 }, w, h, puff: roundness > 0.5 });
}

// eight wide ridges (512x160) and four round puffs (256x256)
for (let i = 0; i < 8; i++) {
    bakeBillow((i & 1) * 512, (i >> 1) * 160, 512, 160, 101 + i * 17, 0.15 * (i % 3));
}
for (let i = 0; i < 4; i++) {
    bakeBillow(i * 256, 640, 256, 256, 401 + i * 23, 1);
}
const atlasTex = G.createTexture(G.createImageData(ATLAS, ATLAS, atlasPx),
    { wrap: 'clamp', mipmaps: true });

// ---- sky and haze gradients --------------------------------------------------
const SKY_ROWS = 256;
const skyPx = new Uint8Array(SKY_ROWS * 4);
for (let y = 0; y < SKY_ROWS; y++) {
    const sy = ((y + 0.5) / SKY_ROWS) * H;
    let c;
    if (sy < HY) {
        const t = Math.pow(sy / HY, 1.6);
        c = [0, 1, 2].map((k) => mix(SKY_TOP[k], SKY_HORIZON[k], t));
    } else {
        const t = smoothstep(HY, H, sy);
        c = [0, 1, 2].map((k) => mix(VALLEY_TOP[k], VALLEY_DEEP[k], t));
    }
    skyPx[y * 4] = c[0] * 255;
    skyPx[y * 4 + 1] = c[1] * 255;
    skyPx[y * 4 + 2] = c[2] * 255;
    skyPx[y * 4 + 3] = 255;
}
const skyTex = G.createTexture(G.createImageData(1, SKY_ROWS, skyPx), { wrap: 'clamp' });

// haze band peaks 40% down its height: short fall into the sky, long into the deck
const hazeTex = bakeAlpha(1, 128, (u, v) =>
    v < 0.4 ? Math.pow(smoothstep(0, 0.4, v), 1.5) : Math.pow(1 - smoothstep(0.4, 1, v), 1.3));

// ---- glow / flare textures ---------------------------------------------------
const glowTex = bakeAlpha(128, 128, (u, v) => {
    const d = Math.hypot(u - 0.5, v - 0.5) * 2;
    return Math.exp(-d * d * 4.5) * (1 - smoothstep(0.85, 1, d));
});
const ringTex = bakeAlpha(128, 128, (u, v) => {
    const d = Math.hypot(u - 0.5, v - 0.5) * 2;
    return Math.exp(-Math.pow((d - 0.82) / 0.07, 2)) + 0.12 * Math.exp(-d * d * 3);
});
const discTex = bakeAlpha(64, 64, (u, v) => {
    const d = Math.hypot(u - 0.5, v - 0.5) * 2;
    return smoothstep(1.0, 0.8, d) * (0.55 + 0.45 * d * d);
});
const rayTex = bakeAlpha(256, 16, (u, v) => {
    const across = (v - 0.5) * 2;
    return Math.exp(-across * across * 6) * Math.pow(1 - u, 1.4) * smoothstep(0, 0.05, u);
});
const streakTex = bakeAlpha(256, 16, (u, v) => {
    const along = (u - 0.5) * 2;
    const across = (v - 0.5) * 2;
    return Math.exp(-across * across * 9) * Math.pow(1 - Math.abs(along), 2.2);
});
const starTex = bakeAlpha(64, 64, (u, v) => {
    const x = (u - 0.5) * 2;
    const y = (v - 0.5) * 2;
    const spike = Math.exp(-Math.abs(x) * 30) * (1 - Math.abs(y))
        + Math.exp(-Math.abs(y) * 30) * (1 - Math.abs(x));
    const core = Math.exp(-(x * x + y * y) * 30);
    return spike * 0.9 + core;
});

const whiteTex = bakeAlpha(1, 1, () => 1);

const logoTex = G.createTexture(G.loadImage('efx_logo.png'), { wrap: 'clamp', mipmaps: true });

G.setClearColor([SKY_HORIZON[0], SKY_HORIZON[1], SKY_HORIZON[2], 1]);
G.setPostEffects([
    { effect: 'bloom', threshold: 0.86, strength: 0.32 },
    { effect: 'colorFilter', saturation: 1.08, contrast: 1.04 },
]);

// ---- the cloud deck ------------------------------------------------------------
// World-uniform density: every billow spawns across the full far-plane
// width and flies straight at the camera, so the near field thins out
// exactly as the frustum narrows. Tall towers only grow clear of the
// flight lane, so nothing ever engulfs the camera.
const clouds = [];

function spawn(c, z) {
    c.z = z;
    c.x = (rand() * 2 - 1) * X_SPAN;
    c.slot = rand() < 0.7 ? (rand() * 8) | 0 : 8 + ((rand() * 4) | 0);
    c.ww = SLOTS[c.slot].puff ? 14 + 14 * rand() : 26 + 30 * rand();
    let bump = -1.8 + 4.4 * Math.pow(rand(), 1.4);
    const clear = Math.abs(c.x) - c.ww * 0.5;
    if (clear > 3 && rand() < 0.28) bump += 2 + 4.5 * rand(); // towers off the lane
    if (clear < 3 && bump > EYE - 1.3) bump = EYE - 1.3 - rand();
    c.top = bump - EYE;
    c.alpha = 0.92 + 0.08 * rand();
}

for (let i = 0; i < CLOUD_COUNT; i++) {
    const c = { z: 0, x: 0, top: 0, ww: 0, slot: 0, alpha: 1 };
    spawn(c, Z_NEAR + (Z_FAR - Z_NEAR) * rand());
    clouds.push(c);
}

const batch = [];
const pool = [];
function sprite(k) {
    while (pool.length <= k) {
        pool.push({ x: 0, y: 0, size: [0, 0], color: [1, 1, 1, 1], sourceRect: null });
    }
    return pool[k];
}
const byZDesc = (a, b) => b.z - a.z;

// ---- logo glints ---------------------------------------------------------------
const glints = [];
for (let i = 0; i < 7; i++) glints.push({ life: 0, age: 1, u: 0, v: 0, size: 0, spin: 0 });
let glintTimer = 0;
const glintRng = makeRng(0x51ab);

const shineBatch = [];
const shinePool = [];
// shine cells in logo texels: 3x6 balances the sweep line's 1 : 0.5 slope
const SHINE_CELL_W = 3;
const SHINE_CELL_H = 6;

let time = 0;

function update(dt) {
    time += dt;
    for (const c of clouds) {
        c.z -= SPEED * dt;
        if (c.z < Z_NEAR) spawn(c, c.z + (Z_FAR - Z_NEAR));
    }
    for (const g of glints) g.age += dt;
    if (time > FLASH_T + 0.3) {
        glintTimer -= dt;
        if (glintTimer <= 0) {
            glintTimer = 0.18 + 0.25 * glintRng();
            const g = glints.find((q) => q.age >= q.life);
            if (g) {
                g.age = 0;
                g.life = 0.45 + 0.35 * glintRng();
                g.u = 0.06 + 0.88 * glintRng();
                g.v = 0.12 + 0.6 * glintRng();
                g.size = LOGO_H * (0.22 + 0.22 * glintRng());
                g.spin = (glintRng() - 0.5) * 120;
            }
        }
    }
}

// ---- render passes -------------------------------------------------------------
function drawSky() {
    G.drawQuad(skyTex, 0, 0, { size: [W, H] });
    // aerial haze straddling the horizon: the far deck melts into it
    const hb = H * 0.24;
    G.drawQuad(hazeTex, 0, HY - hb * 0.4,
        { size: [W, hb], color: [HAZE[0], HAZE[1], HAZE[2], 0.9] });
    // the sun sits just off the top-right edge; its glow and rays live in
    // the sky, behind the deck
    const pulse = 1 + 0.04 * Math.sin(time * 1.7);
    const big = W * 1.25 * pulse;
    G.drawQuad(glowTex, SUN_X - big * 0.5, SUN_Y - big * 0.5,
        { size: [big, big], color: [0.75, 0.88, 1.0, 0.45], blend: 'additive' });
    const mid = W * 0.42;
    G.drawQuad(glowTex, SUN_X - mid * 0.5, SUN_Y - mid * 0.5,
        { size: [mid, mid], color: [1.0, 0.97, 0.9, 0.85], blend: 'additive' });
    for (let i = 0; i < 16; i++) {
        const seed = Math.sin(i * 12.9898) * 43758.5453;
        const jitter = seed - Math.floor(seed);
        const ang = 60 + (i / 16) * 360 + time * (i & 1 ? 2.5 : -1.8);
        const len = W * (0.55 + 0.6 * jitter);
        const th = H * (0.03 + 0.05 * jitter);
        const a = (0.07 + 0.06 * jitter) * (0.75 + 0.25 * Math.sin(time * (0.9 + jitter) + i));
        G.drawQuad(rayTex, SUN_X, SUN_Y - th * 0.5, {
            size: [len, th], rotation: ang, origin: [0, th * 0.5],
            color: [1.0, 0.97, 0.88, a], blend: 'additive',
        });
    }
}

function drawClouds() {
    clouds.sort(byZDesc);
    let n = 0;
    for (const c of clouds) {
        const s = FOCAL / c.z;
        const slot = SLOTS[c.slot];
        const sw = c.ww * s;
        if (sw < 2) continue;
        // a distant billow is seen at a grazing angle: squash it
        const sh = sw * (slot.h / slot.w) * (1.05 - 0.5 * smoothstep(6, 200, c.z));
        const sx = CX + c.x * s - sw * 0.5;
        const sy = HY - c.top * s - sh * 0.05;
        if (sy > H || sx > W || sx + sw < 0) continue;
        const haze = Math.pow(smoothstep(25, Z_FAR * 0.95, c.z), 0.8);
        // melt into the haze band behind the deck; fade in at the far end
        const fade = smoothstep(Z_FAR, Z_FAR * 0.9, c.z)
            * (1 - 0.6 * smoothstep(Z_FAR * 0.3, Z_FAR, c.z));
        const e = sprite(n);
        e.x = sx; e.y = sy;
        e.size[0] = sw; e.size[1] = sh;
        e.sourceRect = slot.rect;
        e.color[0] = mix(1, HAZE[0] * 0.97, haze);
        e.color[1] = mix(1, HAZE[1] * 0.99, haze);
        e.color[2] = 1;
        e.color[3] = c.alpha * fade;
        batch[n++] = e;
    }
    batch.length = n;
    if (n) G.drawSprites(atlasTex, batch);
}

function drawFlare() {
    const fx = CX - SUN_X;
    const fy = H * 0.5 - SUN_Y;
    const core = W * 0.16 * (1 + 0.05 * Math.sin(time * 3.1));
    G.drawQuad(glowTex, SUN_X - core * 0.5, SUN_Y - core * 0.5,
        { size: [core, core], color: [1, 1, 1, 0.9], blend: 'additive' });
    // anamorphic streak through the sun
    const sl = W * 1.5;
    const st = H * 0.025;
    G.drawQuad(streakTex, SUN_X - sl * 0.5, SUN_Y - st * 0.5,
        { size: [sl, st], color: [0.55, 0.78, 1.0, 0.55], blend: 'additive' });
    const ghosts = [
        [0.30, 0.05, discTex, 0.55, 0.85, 1.00, 0.10],
        [0.52, 0.09, ringTex, 0.70, 0.90, 1.00, 0.09],
        [0.74, 0.03, discTex, 1.00, 0.85, 0.55, 0.16],
        [1.10, 0.07, discTex, 0.60, 1.00, 0.80, 0.08],
        [1.38, 0.16, ringTex, 0.80, 0.70, 1.00, 0.06],
        [1.62, 0.04, discTex, 1.00, 0.75, 0.55, 0.14],
    ];
    for (const g of ghosts) {
        const size = W * g[1];
        const gx = SUN_X + fx * g[0];
        const gy = SUN_Y + fy * g[0];
        G.drawQuad(g[2], gx - size * 0.5, gy - size * 0.5,
            { size: [size, size], color: [g[3], g[4], g[5], g[6]], blend: 'additive' });
    }
}

function drawLogo() {
    if (time < LOGO_IN) return;
    const pop = clamp01((time - LOGO_IN) / 1.1);
    const scale = 0.25 + 0.75 * elasticOut(pop);
    const appear = smoothstep(0, 0.25, pop);
    const ly = LOGO_Y + LOGO_BOB * Math.sin((time * 2 * Math.PI) / LOGO_PERIOD);
    const lw = LOGO_W * scale;
    const lh = LOGO_H * scale;
    const lx0 = CX - lw * 0.5;
    const ly0 = ly - lh * 0.5;

    // halo behind: lifts the sky around the badge
    const halo = 0.30 + 0.06 * Math.sin(time * 2.2);
    G.drawQuad(glowTex, CX - lw * 0.85, ly - lh * 1.25,
        { size: [lw * 1.7, lh * 2.5], color: [0.65, 0.85, 1.0, halo * appear], blend: 'additive' });
    // soft drop shadow: a ring of faint offset copies
    for (let i = 0; i < 8; i++) {
        const a = (i / 8) * 2 * Math.PI;
        G.drawQuad(logoTex, lx0 + Math.cos(a) * 5 * scale + 4 * scale,
            ly0 + Math.sin(a) * 5 * scale + 9 * scale,
            { size: [lw, lh], color: [0.02, 0.12, 0.35, 0.07 * appear] });
    }
    G.drawQuad(logoTex, lx0, ly0, { size: [lw, lh], color: [1, 1, 1, appear] });

    // arrival flash: white burst through the logo and a shock ring
    if (time > FLASH_T) {
        const ft = time - FLASH_T;
        const f = Math.exp(-ft * 4.5);
        if (f > 0.01) {
            G.drawQuad(logoTex, lx0, ly0,
                { size: [lw, lh], color: [1, 1, 1, f], blend: 'additive' });
            const rr = LOGO_W * (0.5 + 1.8 * ft);
            G.drawQuad(ringTex, CX - rr * 0.5, ly - rr * 0.5,
                { size: [rr, rr], color: [0.85, 0.95, 1.0, f * 0.8], blend: 'additive' });
        }
    }

    // diagonal shine sweep, masked by the logo's own alpha: the logo is
    // re-drawn additively in small source-rect cells near the sweep line
    if (time > SHINE_START) {
        const p = ((time - SHINE_START) % SHINE_PERIOD) / 0.9;
        if (p < 1) {
            const band = 80;
            const s = -band - 140 + p * (619 + 2 * band + 140);
            const kx = lw / 619;
            const ky = lh / 265;
            let n = 0;
            for (let ty = 0; ty < 265; ty += SHINE_CELL_H) {
                const ch = Math.min(SHINE_CELL_H, 265 - ty);
                const yc = 0.5 * (ty + ch * 0.5);
                const tx0 = Math.max(0, Math.floor((s - band - yc) / SHINE_CELL_W) * SHINE_CELL_W);
                const tx1 = Math.min(619, s + band - yc);
                for (let tx = tx0; tx < tx1; tx += SHINE_CELL_W) {
                    const cw = Math.min(SHINE_CELL_W, 619 - tx);
                    const d = (tx + cw * 0.5 + yc - s) / 34;
                    const intensity = Math.exp(-d * d) * 0.85;
                    if (intensity < 0.02) continue;
                    while (shinePool.length <= n) {
                        shinePool.push({ x: 0, y: 0, size: [0, 0], color: [1, 1, 1, 1],
                            sourceRect: { x: 0, y: 0, w: 1, h: 1 } });
                    }
                    const e = shinePool[n];
                    e.x = lx0 + tx * kx; e.y = ly0 + ty * ky;
                    e.size[0] = cw * kx; e.size[1] = ch * ky;
                    e.sourceRect.x = tx; e.sourceRect.y = ty;
                    e.sourceRect.w = cw; e.sourceRect.h = ch;
                    e.color[3] = intensity;
                    shineBatch[n++] = e;
                }
            }
            shineBatch.length = n;
            if (n) G.drawSprites(logoTex, shineBatch, { blend: 'additive' });
        }
    }

    // twinkling star glints on the badge
    for (const g of glints) {
        if (g.age >= g.life) continue;
        const t = g.age / g.life;
        const a = Math.sin(Math.PI * t);
        const size = g.size * (0.4 + 0.6 * a);
        G.drawQuad(starTex, lx0 + g.u * lw - size * 0.5, ly0 + g.v * lh - size * 0.5, {
            size: [size, size], rotation: g.spin * t,
            color: [1, 1, 1, a], blend: 'additive',
        });
    }
}

function render() {
    drawSky();
    drawClouds();
    drawFlare();
    drawLogo();
    // fade in from a white-out
    const w = 1 - smoothstep(0.0, 0.8, time);
    if (w > 0.001) G.drawQuad(whiteTex, 0, 0, { size: [W, H], color: [1, 1, 1, w] });
}
