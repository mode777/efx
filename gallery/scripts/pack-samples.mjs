#!/usr/bin/env node
// Build the release curated-samples archive: one `<name>/` folder per curated
// sample containing that sample's `main.js` and resources, plus a root
// `CREDITS.md` for provenance. It is derived from the same sample directories
// the gallery build packs, so there is no separate source of truth
// (design D1/D2/D7).
//
// Usage:
//   node gallery/scripts/pack-samples.mjs [--out <path>]
// Env:
//   EFX_VERSION  version token for the default file name (default: "dev")
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';
import { readCurated, packSamples, curatedDir } from './lib/samples.mjs';

const here = path.dirname(url.fileURLToPath(import.meta.url));
const galleryDir = path.resolve(here, '..');
const repoRoot = path.resolve(galleryDir, '..');

function flag(name, fallback) {
    const i = process.argv.indexOf(name);
    return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : fallback;
}

const version = process.env.EFX_VERSION || 'dev';
const out = path.resolve(
    repoRoot,
    flag('--out', path.join('dist', `emotion-fx-${version}-samples.zip`))
);

const entries = readCurated();
const credits = fs.readFileSync(path.join(curatedDir, 'CREDITS.md'));
const data = packSamples(entries, [{ name: 'CREDITS.md', data: credits }]);

fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, data);
console.log(
    `pack-samples: ${entries.length} samples -> ${path.relative(repoRoot, out)} (${data.length} bytes)`
);
