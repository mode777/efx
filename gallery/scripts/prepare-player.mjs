#!/usr/bin/env node
// Copies the Emscripten web player output into the gallery's static assets.
// Source dir defaults to ../build-web (the Pages workflow's emcmake build);
// override with EFX_WEB_BUILD. The copied files are served to the runner
// iframe at ./player/player_web.js.
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const here = path.dirname(url.fileURLToPath(import.meta.url));
const galleryDir = path.resolve(here, '..');
const repoRoot = path.resolve(galleryDir, '..');
// Resolve EFX_WEB_BUILD against the repo root (not the script cwd), so
// `EFX_WEB_BUILD=build-web npm --prefix gallery run prepare:player` works
// regardless of the invoking directory (npm sets cwd to the package dir).
const srcEnv = process.env.EFX_WEB_BUILD || path.join('..', 'build-web');
const src = path.isAbsolute(srcEnv) ? srcEnv : path.resolve(repoRoot, srcEnv);
const dest = path.join(galleryDir, 'public', 'player');
const files = ['player_web.js', 'player_web.wasm', 'player_web.data'];

if (!fs.existsSync(src)) {
    console.error(`prepare-player: build dir not found: ${src}`);
    process.exit(1);
}
const missing = files.filter((f) => !fs.existsSync(path.join(src, f)));
if (missing.length) {
    console.error(`prepare-player: missing build output(s): ${missing.join(', ')}`);
    process.exit(1);
}
fs.mkdirSync(dest, { recursive: true });
for (const f of files) {
    fs.copyFileSync(path.join(src, f), path.join(dest, f));
}
console.log(`prepare-player: copied ${files.join(', ')} -> ${path.relative(galleryDir, dest)}`);
