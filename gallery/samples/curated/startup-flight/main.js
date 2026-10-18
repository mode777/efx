// Startup Flight: a boot-screen-style flight along the edge of a cloud deck
// under a clear blue sky, with the EFX logo swaying gently in the center.
// Everything on screen is a 2D drawQuad in painter's order: the sky gradient
// is recorded first, clouds far to near, the logo last. Perspective (screen
// position and size) is computed in JS from focal*x/z. Every cloud sprite is
// sculpted per-pixel at load from seeded fBm noise (efx.noise) — the logo is
// the only image asset.

// ---- palette ------------------------------------------------------------
const SKY_ZENITH = [0.13, 0.38, 0.78];   // deep blue overhead
const SKY_HORIZON = [0.80, 0.89, 0.97];  // pale haze at the vanishing point
const SKY_UNDER = [0.58, 0.69, 0.84];    // undercast haze below the deck
const PUFF_SHADOW = [0.90, 0.93, 0.97];  // blue-gray underside shading
const WISP_TINT = [0.93, 0.96, 1.0];

// ---- layout: derived from the live surface at boot -----------------------
const W = efx.window.width || 640;
const H = efx.window.height || 480;
const CX = W * 0.5;
const CY = H * 0.5;
const HORIZON_Y = H * 0.42; // vanishing point above center: slight pitch-down
const FOCAL = (H * 0.5) / Math.tan(30 * Math.PI / 180); // 60 deg vertical fov

// ---- flight ---------------------------------------------------------------
const CLOUD_SPEED = 10;      // world units/s toward the camera
const Z_RESPAWN = 2.5;       // teleport to the far end below this distance
const Z_FAR = 110;
const FAR_FADE_START = 0.85; // fraction of Z_FAR where the fade-in begins
const NEAR_FADE_END = 6;     // fully opaque beyond this distance
const BANK_AMPL = 0.035;     // bank roll, radians (~2 deg)
const BANK_PERIOD = 16;      // seconds per bank cycle

// ---- logo -----------------------------------------------------------------
const LOGO_W = W * 0.46;
const LOGO_H = (LOGO_W * 265) / 619; // efx_logo.png is 619x265
const LOGO_SWAY = H * 0.015;
const LOGO_PERIOD = 3.5;

// ---- deterministic randomness (fixed seed; no Math.random) ---------------
let rng = 0x2f6e2b1;
function rand() {
    rng = (rng * 1664525 + 1013904223) >>> 0;
    return rng / 4294967296;
}

function clamp01(x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
function smoothstep(a, b, x) {
    const t = clamp01((x - a) / (b - a));
    return t * t * (3 - 2 * t);
}
function mix3(a, b, t) {
    return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];
}
// the sky gradient's color at a screen row, so clouds can dissolve into the
// sky color at their own height (aerial perspective), not one global haze
function skyColorAt(sy) {
    const t = clamp01(sy / H);
    const hz = HORIZON_Y / H;
    return t < hz
        ? mix3(SKY_ZENITH, SKY_HORIZON, smoothstep(0, hz, t))
        : mix3(SKY_HORIZON, SKY_UNDER, smoothstep(hz, 1, t));
}

// ---- procedural cloud sprites ---------------------------------------------
// Puffs are one smooth lobed mass: the silhouette radius wobbles with
// periodic ring-sampled noise (cumulus lobes), the interior stays solid so
// mip filtering can never produce lace, and shading bakes into RGB only at
// the lowest band (2D quads are unlit). Wisps keep a streaky noise carve.
const TEX = 256;

function makePuff(seed, lobesFreq, lobeAmp, ax, ay, bottomBoost) {
    const mott = new Float32Array(TEX * TEX);
    efx.noise.fill2(mott, TEX, TEX, {
        seed: seed + 31, type: 'opensimplex2', frequency: 0.014,
        octaves: 3, lacunarity: 2.0, gain: 0.5,
    });
    const LUTN = 720;
    const lobe = new Float32Array(LUTN);
    for (let i = 0; i < LUTN; i++) {
        const a = (i / LUTN) * Math.PI * 2;
        const n1 = efx.noise.noise2(Math.cos(a) * lobesFreq, Math.sin(a) * lobesFreq, { seed });
        const n2 = efx.noise.noise2(Math.cos(a) * lobesFreq * 2.3, Math.sin(a) * lobesFreq * 2.3,
            { seed: seed + 7 });
        lobe[i] = 1 + lobeAmp * n1 + lobeAmp * 0.35 * n2;
    }
    const px = new Uint8Array(TEX * TEX * 4);
    for (let y = 0; y < TEX; y++) {
        const ny = (y / (TEX - 1)) * 2 - 1; // -1 top, +1 bottom
        const ny01 = (ny + 1) * 0.5;
        const b2 = Math.max(0, ny01 - 0.68) / 0.32;
        const bottom = b2 * b2 * bottomBoost;
        for (let x = 0; x < TEX; x++) {
            const nx = (x / (TEX - 1)) * 2 - 1;
            const ex = nx / ax, ey = ny / ay;
            const r = Math.sqrt(ex * ex + ey * ey);
            const ang = Math.atan2(ey, ex);
            const li = Math.round(((ang + Math.PI) / (Math.PI * 2)) * LUTN) % LUTN;
            let alpha = smoothstep(1.0, 0.80, r / lobe[li]);
            alpha *= 0.955 + 0.045 * (mott[y * TEX + x] * 0.5 + 0.5);
            const i = (y * TEX + x) * 4;
            const shade = bottom * alpha * alpha; // partial-alpha rim stays white
            px[i] = 255 * (1 + (PUFF_SHADOW[0] - 1) * shade);
            px[i + 1] = 255 * (1 + (PUFF_SHADOW[1] - 1) * shade);
            px[i + 2] = 255 * (1 + (PUFF_SHADOW[2] - 1) * shade);
            px[i + 3] = alpha * 255;
        }
    }
    return efx.graphics.createTexture(efx.graphics.createImageData(TEX, TEX, px),
        { wrap: 'clamp', mipmaps: true });
}

function makeWisp(seed, freq, ax, ay) {
    const buf = new Float32Array(TEX * TEX);
    efx.noise.fill2(buf, TEX, TEX, {
        seed, type: 'opensimplex2', frequency: freq, octaves: 4, lacunarity: 2.0, gain: 0.5,
    });
    const px = new Uint8Array(TEX * TEX * 4);
    for (let y = 0; y < TEX; y++) {
        const ny = (y / (TEX - 1)) * 2 - 1;
        for (let x = 0; x < TEX; x++) {
            const nx = (x / (TEX - 1)) * 2 - 1;
            const d2 = (nx * nx) / (ax * ax) + (ny * ny) / (ay * ay);
            const fall = 1 - d2;
            const fallS = fall <= 0 ? 0 : smoothstep(0, 0.4, fall);
            // carve around the median: |n| p75~0.24 (measured), so streaks
            // cover ~a quarter of the mask
            const an = Math.abs(buf[y * TEX + x]);
            const dens = smoothstep(0.18, 0.5, (an * 1.9 - 0.2) * fallS);
            const i = (y * TEX + x) * 4;
            px[i] = 255; px[i + 1] = 255; px[i + 2] = 255;
            px[i + 3] = dens * 255;
        }
    }
    return efx.graphics.createTexture(efx.graphics.createImageData(TEX, TEX, px),
        { wrap: 'clamp', mipmaps: true });
}

const PUFFS = [];
const PUFF_SEEDS = [11, 23, 37, 41, 53];
for (let i = 0; i < PUFF_SEEDS.length; i++) {
    PUFFS.push(makePuff(PUFF_SEEDS[i], 1.3 + 0.35 * i, 0.10,
        0.92, 0.88, 0.35));
}
const WISPS = [
    makeWisp(7, 0.013, 0.98, 0.40),
    makeWisp(19, 0.017, 0.98, 0.46),
];

// ---- sky gradient ----------------------------------------------------------
// Baked as rows; stop positions account for the sky quad being drawn
// oversized so the bank rotation never reveals a corner.
const SKY_TEX_H = 256;
const SKY_Q = Math.sqrt(W * W + H * H) * 1.06;
const skyPx = new Uint8Array(4 * SKY_TEX_H);
const horizonRow = SKY_TEX_H * 0.5 + ((HORIZON_Y - CY) * SKY_TEX_H) / SKY_Q;
for (let y = 0; y < SKY_TEX_H; y++) {
    const c = y < horizonRow
        ? mix3(SKY_ZENITH, SKY_HORIZON, smoothstep(0, horizonRow, y))
        : mix3(SKY_HORIZON, SKY_UNDER, smoothstep(horizonRow, SKY_TEX_H, y));
    skyPx[y * 4] = c[0] * 255;
    skyPx[y * 4 + 1] = c[1] * 255;
    skyPx[y * 4 + 2] = c[2] * 255;
    skyPx[y * 4 + 3] = 255;
}
const skyTex = efx.graphics.createTexture(
    efx.graphics.createImageData(1, SKY_TEX_H, skyPx), { wrap: 'clamp' });

const logoTex = efx.graphics.createTexture(efx.graphics.loadImage('efx_logo.png'),
    { wrap: 'clamp', mipmaps: true });

efx.graphics.setClearColor([SKY_HORIZON[0], SKY_HORIZON[1], SKY_HORIZON[2], 1]);

// ---- the field --------------------------------------------------------------
// Three bands: a broken deck below the route (the edge of the clouds), hero
// puffs at eye level, and faint wisps high above.
const clouds = [];

function respawn(c, z) {
    c.z = z;
    if (c.band === 0) {          // deck
        c.x = (rand() * 2 - 1) * (34 + 66 * (c.z / Z_FAR));
        c.y = -24 - rand() * 12;
        c.w = 32 + rand() * 30;
        c.aspect = 0.40 + rand() * 0.18;
        c.tex = PUFFS[(rand() * PUFFS.length) | 0];
        c.tint = null;
    } else if (c.band === 1) {   // eye-level puffs
        c.x = (rand() * 2 - 1) * (10 + 14 * (c.z / Z_FAR));
        c.y = -6 + rand() * 10;
        c.w = 8 + rand() * 7;
        c.aspect = 0.58 + rand() * 0.12;
        c.tex = PUFFS[(rand() * PUFFS.length) | 0];
        c.tint = null;
    } else {                     // high wisps
        c.x = (rand() * 2 - 1) * (26 + 38 * (c.z / Z_FAR));
        c.y = 16 + rand() * 16;
        c.w = 44 + rand() * 28;
        c.aspect = 0.17 + rand() * 0.07;
        c.tex = WISPS[(rand() * WISPS.length) | 0];
        c.tint = WISP_TINT;
    }
}

function addBand(band, count) {
    for (let i = 0; i < count; i++) {
        const c = { band, z: 0, x: 0, y: 0, w: 0, aspect: 1, tex: null, tint: null };
        respawn(c, Z_RESPAWN + ((Z_FAR - Z_RESPAWN) * (i + rand() * 0.8)) / count);
        clouds.push(c);
    }
}
addBand(0, 16);
addBand(1, 12);
addBand(2, 8);

const byZDesc = (a, b) => b.z - a.z;
const tintScratch = [1, 1, 1, 1];
let time = 0;

function update(dt) {
    time += dt;
    for (const c of clouds) {
        c.z -= CLOUD_SPEED * dt;
        if (c.z < Z_RESPAWN) respawn(c, Z_FAR - (Z_RESPAWN - c.z));
    }
}

function render() {
    const bank = BANK_AMPL * Math.sin((time * 2 * Math.PI) / BANK_PERIOD);
    const bankDeg = (bank * 180) / Math.PI;
    const cb = Math.cos(bank);
    const sb = Math.sin(bank);

    // sky first; oversized so the bank tilt never shows a corner
    efx.graphics.drawQuad(skyTex, CX - SKY_Q * 0.5, CY - SKY_Q * 0.5,
        { size: [SKY_Q, SKY_Q], rotation: bankDeg });

    clouds.sort(byZDesc);
    for (const c of clouds) {
        const rx = c.x * cb - c.y * sb; // bank rotates the world, not the sprites
        const ry = c.x * sb + c.y * cb;
        const inv = 1 / c.z;
        const w = FOCAL * c.w * inv;
        if (w < 1.5) continue;
        const h = w * c.aspect;
        const sx = CX + FOCAL * rx * inv;
        const sy = HORIZON_Y - FOCAL * ry * inv;
        const farT = smoothstep(Z_FAR * FAR_FADE_START, Z_FAR * 0.98, c.z);
        const a = (1 - farT) * smoothstep(Z_RESPAWN, NEAR_FADE_END, c.z);
        if (a <= 0.003) continue;
        // aerial perspective: gentle distance whiten plus a strong dissolve
        // into the sky color inside the horizon haze band
        const zT = smoothstep(0.55, 0.95, c.z / Z_FAR) * 0.30;
        const bandT = smoothstep(0.22, 0.04, Math.abs(sy - HORIZON_Y) / H) * 0.8;
        const hazeT = Math.min(1, zT + bandT);
        const sky = skyColorAt(sy);
        const tint = c.tint;
        const tr = tint ? tint[0] : 1;
        const tg = tint ? tint[1] : 1;
        const tb = tint ? tint[2] : 1;
        tintScratch[0] = tr + (sky[0] - tr) * hazeT;
        tintScratch[1] = tg + (sky[1] - tg) * hazeT;
        tintScratch[2] = tb + (sky[2] - tb) * hazeT;
        tintScratch[3] = a * (tint ? 0.5 : 1);
        efx.graphics.drawQuad(c.tex, sx - w * 0.5, sy - h * 0.5,
            { size: [w, h], color: tintScratch });
    }

    // logo last: always on top
    const ly = CY + LOGO_SWAY * Math.sin((time * 2 * Math.PI) / LOGO_PERIOD);
    efx.graphics.drawQuad(logoTex, CX - LOGO_W * 0.5, ly - LOGO_H * 0.5,
        { size: [LOGO_W, LOGO_H] });
}
