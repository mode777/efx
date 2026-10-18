// Startup Flight: a boot-screen-style flight along the upper edge of a sea
// of clouds under a saturated blue sky, with the EFX logo swaying gently in
// the center. Everything on screen is a 2D drawQuad: the sky gradient is
// recorded first, then the cloud sea with additive blending (overlap
// accumulates toward sunlit white; thin coverage shades toward the sky
// color; gaps show the dark under-color as holes), the logo last.
// Perspective (screen position and size) is computed in JS from focal*x/z.
// Every cloud texture is sculpted per-pixel at load from seeded fBm noise
// (efx.noise) — the logo is the only image asset.

// ---- palette ------------------------------------------------------------
const SKY_ZENITH = [0.10, 0.42, 0.90];   // saturated Sega blue overhead
const SKY_HORIZON = [0.62, 0.82, 1.00];  // pale haze at the vanishing point
const SKY_UNDER_TOP = [0.52, 0.65, 0.84]; // haze shadow just under the deck
const SKY_UNDER_DEEP = [0.12, 0.22, 0.42]; // deep gaps toward frame bottom
const CLOUD_TINT = [0.96, 0.98, 1.00];   // thin: shadow blue; stacked: white

// ---- layout: derived from the live surface at boot -----------------------
const W = efx.window.width || 640;
const H = efx.window.height || 480;
const CX = W * 0.5;
const CY = H * 0.5;
const HORIZON_Y = H * 0.42; // vanishing point above center: slight pitch-down
const FOCAL = (H * 0.5) / Math.tan(30 * Math.PI / 180); // 60 deg vertical fov

// ---- flight ---------------------------------------------------------------
const SEA_SPEED = 10;        // world units/s toward the camera
const Z_RESPAWN = 2.5;       // teleport to the far end below this distance
const Z_FAR = 110;
const FAR_FADE_START = 0.62; // fraction of Z_FAR where the haze blend begins
const NEAR_FADE_END = 9;     // fully contributing beyond this distance
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
// the sky gradient's color at a screen row, so distant clouds dissolve into
// the sky color at their own height (aerial perspective)
function skyColorAt(sy) {
    const t = clamp01(sy / H);
    const hz = HORIZON_Y / H;
    if (t < hz) {
        return mix3(SKY_ZENITH, SKY_HORIZON, smoothstep(0, hz, t));
    }
    return mix3(SKY_UNDER_TOP, SKY_UNDER_DEEP, smoothstep(hz, 1, t));
}

// ---- procedural billow textures --------------------------------------------
// A billow is a soft density field, not a cutout: a broad dome envelope
// modulated by |fBm| detail. Additive blending stacks the fields — overlap
// accumulates toward white, sparse coverage reads as blue shadow — so the
// sprite needs no silhouette and can never show a boundary.
const TEX = 256;

function makeBillow(seed, freq, octaves) {
    const buf = new Float32Array(TEX * TEX);
    efx.noise.fill2(buf, TEX, TEX, {
        seed, type: 'opensimplex2', frequency: freq,
        octaves, lacunarity: 2.0, gain: 0.5,
    });
    const px = new Uint8Array(TEX * TEX * 4);
    for (let y = 0; y < TEX; y++) {
        const ny = (y / (TEX - 1)) * 2 - 1;
        for (let x = 0; x < TEX; x++) {
            const nx = (x / (TEX - 1)) * 2 - 1;
            const r01 = Math.sqrt(nx * nx + ny * ny);
            const env = smoothstep(1.0, 0.12, r01); // broad dome, wide skirt
            // |fBm| detail: measured |n| p90~0.43, p97~0.57, so most texels
            // carry structure and only creases fall to the base density
            let b = Math.abs(buf[y * TEX + x]) * 1.9;
            if (b > 1) b = 1;
            const d = env * (0.42 + 0.88 * b);
            const i = (y * TEX + x) * 4;
            px[i] = 255; px[i + 1] = 255; px[i + 2] = 255;
            px[i + 3] = d > 1 ? 255 : d * 255;
        }
    }
    return efx.graphics.createTexture(efx.graphics.createImageData(TEX, TEX, px),
        { wrap: 'clamp', mipmaps: true });
}

// six variants: two broad soft masses, two mid billows, two fine near puffs
const SEA_TEX = [
    makeBillow(11, 0.013, 4),
    makeBillow(23, 0.016, 4),
    makeBillow(37, 0.021, 4),
    makeBillow(41, 0.026, 5),
    makeBillow(53, 0.032, 5),
    makeBillow(67, 0.040, 5),
];

// ---- sky gradient ----------------------------------------------------------
// Baked as rows; stop positions account for the sky quad being drawn
// oversized so the bank rotation never reveals a corner. Below the horizon
// the gradient falls into cloud-shadow blues: gaps in the additive sea show
// this under-color and read as holes in the deck.
const SKY_TEX_H = 256;
const SKY_Q = Math.sqrt(W * W + H * H) * 1.06;
const skyPx = new Uint8Array(4 * SKY_TEX_H);
const horizonRow = SKY_TEX_H * 0.5 + ((HORIZON_Y - CY) * SKY_TEX_H) / SKY_Q;
for (let y = 0; y < SKY_TEX_H; y++) {
    let c;
    if (y < horizonRow) {
        c = mix3(SKY_ZENITH, SKY_HORIZON, smoothstep(0, horizonRow, y));
    } else {
        c = mix3(SKY_UNDER_TOP, SKY_UNDER_DEEP, smoothstep(horizonRow, SKY_TEX_H, y));
    }
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

// ---- the sea ---------------------------------------------------------------
// Three depth tiers shape the upper edge: big faint far masses keep the
// horizon line solid, mid billows carry the visible topography, and the
// sparse near tier forms the meandering rim the flight path skims. A thin
// extra-wide tier hugs the horizon line and bridges sky into sea.
const sea = [];

// place a billow so its top edge sits at topWorld (world y, 0 = eye level):
// the far floor stays pinned just under the horizon, while mid and near tops
// rise a little above it and give the sea's rim its billowed silhouette
function setTop(c, topWorld) {
    c.y = topWorld - c.w * c.aspect * 0.5;
}

function respawn(c, z) {
    c.z = z;
    c.x = (rand() * 2 - 1) * (26 + 74 * (z / Z_FAR));
    c.tex = SEA_TEX[(rand() * SEA_TEX.length) | 0];
    if (z > 55) {                 // far floor: broad, faint, continuous
        c.w = 55 + rand() * 45;
        c.aspect = 0.35 + rand() * 0.15;
        c.alpha = 0.13 + rand() * 0.07;
        setTop(c, -(1 + rand() * 8));
    } else if (z > 20) {          // mid billows: the visible topography
        c.w = 44 + rand() * 36;
        c.aspect = 0.50 + rand() * 0.25;
        c.alpha = 0.34 + rand() * 0.26;
        // world-top shrinks with distance so the poke above the horizon
        // stays a modest, roughly constant number of pixels
        setTop(c, (0.5 + rand() * 4) * (z / 45));
    } else {                      // near rim: tall tops brushing the path
        c.w = 50 + rand() * 40;
        c.aspect = 0.52 + rand() * 0.26;
        c.alpha = 0.34 + rand() * 0.24;
        c.tex = SEA_TEX[3 + ((rand() * 3) | 0)]; // fine variants near the eye
        setTop(c, (0.5 + rand() * 3) * (z / 20));
    }
}

// crevice shadows: additive can only brighten, so the sea's interior
// topography is drawn as a dark source-over pass sandwiched between the
// saturated far floor and the additive mid/near highlights
function respawnShadow(c, z) {
    c.z = z;
    c.shadow = 1;
    c.x = (rand() * 2 - 1) * (26 + 74 * (z / Z_FAR));
    c.w = 50 + rand() * 55;
    c.aspect = 0.30 + rand() * 0.20;
    c.tex = SEA_TEX[(rand() * SEA_TEX.length) | 0];
    c.alpha = 0.20 + rand() * 0.18;
    setTop(c, -(2 + rand() * 7) * (z / 45));
}

function spawnHorizonBand(c) {    // extra-wide haze bridge at the horizon
    c.z = 88 + rand() * 18;
    c.x = (rand() * 2 - 1) * 95;
    c.w = 140 + rand() * 80;
    c.aspect = 0.07 + rand() * 0.04;
    c.tex = SEA_TEX[(rand() * 2) | 0]; // broad soft variants only
    c.alpha = 0.09 + rand() * 0.05;
    setTop(c, 1 + rand() * 4); // straddle the vanishing point: bridge haze
    c.shadow = 0;
}

function addSea(count) {
    for (let i = 0; i < count; i++) {
        const c = { z: 0, x: 0, y: 0, w: 0, aspect: 1, tex: null, alpha: 1 };
        const u = (i + rand() * 0.8) / count;
        const z = u < 0.30 ? 55 + (u / 0.30) * (Z_FAR - 60)
            : u < 0.78 ? 22 + ((u - 0.30) / 0.48) * 28
                : Z_RESPAWN + 6 + ((u - 0.78) / 0.22) * 10;
        respawn(c, z);
        c.shadow = 0;
        sea.push(c);
    }
    for (let i = 0; i < 34; i++) {
        const c = { z: 0, x: 0, y: 0, w: 0, aspect: 1, tex: null, alpha: 1, shadow: 0 };
        respawnShadow(c, 15 + rand() * 55);
        sea.push(c);
    }
    for (let i = 0; i < 6; i++) {
        const c = { z: 0, x: 0, y: 0, w: 0, aspect: 1, tex: null, alpha: 1 };
        spawnHorizonBand(c);
        sea.push(c);
    }
}
addSea(130);

const tintScratch = [1, 1, 1, 1];
let time = 0;

function update(dt) {
    time += dt;
    for (const c of sea) {
        c.z -= SEA_SPEED * dt;
        if (c.z < Z_RESPAWN) {
            // recycled billows return as far floor (the horizon band stays put)
            if (c.shadow) {
                respawnShadow(c, Z_FAR - (Z_RESPAWN - c.z));
            } else {
                respawn(c, Z_FAR - (Z_RESPAWN - c.z));
            }
        }
    }
}

function drawCloud(c, pass, cb, sb) {
    const isShadow = !!c.shadow;
    // pass routing: 0 = far floor + horizon band, 1 = shadows, 2 = mid/near
    if (pass === 0 && (isShadow || c.z <= 55)) return;
    if (pass === 1 && !isShadow) return;
    if (pass === 2 && (isShadow || c.z > 55)) return;

    const rx = c.x * cb - c.y * sb; // bank rotates the world, not the billows
    const ry = c.x * sb + c.y * cb;
    const inv = 1 / c.z;
    const w = FOCAL * c.w * inv;
    if (w < 1.5) return;
    const h = w * c.aspect;
    const sx = CX + FOCAL * rx * inv;
    const sy = HORIZON_Y - FOCAL * ry * inv;
    const farT = smoothstep(Z_FAR * FAR_FADE_START, Z_FAR * 0.98, c.z);
    const a = c.alpha
        * (1 - farT * 0.55) // thin out ahead, but keep feeding the horizon band
        * smoothstep(Z_RESPAWN, NEAR_FADE_END, c.z);
    if (a <= 0.004) return;
    const sky = skyColorAt(sy);
    if (isShadow) {
        // cool cloud-shadow tone; recedes with distance like the rest
        tintScratch[0] = 0.52 + (sky[0] - 0.52) * farT;
        tintScratch[1] = 0.63 + (sky[1] - 0.63) * farT;
        tintScratch[2] = 0.80 + (sky[2] - 0.80) * farT;
        tintScratch[3] = a;
        efx.graphics.drawQuad(c.tex, sx - w * 0.5, sy - h * 0.5,
            { size: [w, h], color: tintScratch });
        return;
    }
    tintScratch[0] = CLOUD_TINT[0] + (sky[0] - CLOUD_TINT[0]) * farT;
    tintScratch[1] = CLOUD_TINT[1] + (sky[1] - CLOUD_TINT[1]) * farT;
    tintScratch[2] = CLOUD_TINT[2] + (sky[2] - CLOUD_TINT[2]) * farT;
    tintScratch[3] = a;
    efx.graphics.drawQuad(c.tex, sx - w * 0.5, sy - h * 0.5,
        { size: [w, h], color: tintScratch, blend: 'additive' });
}

function render() {
    const bank = BANK_AMPL * Math.sin((time * 2 * Math.PI) / BANK_PERIOD);
    const bankDeg = (bank * 180) / Math.PI;
    const cb = Math.cos(bank);
    const sb = Math.sin(bank);

    // sky first; oversized so the bank tilt never shows a corner
    efx.graphics.drawQuad(skyTex, CX - SKY_Q * 0.5, CY - SKY_Q * 0.5,
        { size: [SKY_Q, SKY_Q], rotation: bankDeg });

    // pass 1: far floor + horizon band, additive
    for (const c of sea) drawCloud(c, 0, cb, sb);
    // pass 2: mid/near highlights, additive
    for (const c of sea) drawCloud(c, 2, cb, sb);
    // pass 3: cloud-shadow dapple on top (source-over) — the interior
    // topography; additive alone can only brighten, never shade
    for (const c of sea) drawCloud(c, 1, cb, sb);

    // logo last: always on top
    const ly = CY + LOGO_SWAY * Math.sin((time * 2 * Math.PI) / LOGO_PERIOD);
    efx.graphics.drawQuad(logoTex, CX - LOGO_W * 0.5, ly - LOGO_H * 0.5,
        { size: [LOGO_W, LOGO_H] });
}
