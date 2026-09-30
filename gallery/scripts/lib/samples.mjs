// Shared curated-sample helpers. The curated sample directory is the single
// source of truth: `packSample` derives the gallery's mountable resource pack
// (contents at the archive root) and `packSamples` derives the release
// archive (each `<name>/**` under a `<name>/` prefix) from the same
// directories (design D1/D2).
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';
import { makeZip } from './zip.mjs';

const here = path.dirname(url.fileURLToPath(import.meta.url));
export const galleryDir = path.resolve(here, '..', '..');
export const curatedDir = path.join(galleryDir, 'samples', 'curated');

// Editor/tool noise that must never leak into a sample pack.
const SKIP = new Set(['Thumbs.db', 'desktop.ini']);

function isSkipped(name) {
    return name.startsWith('.') || SKIP.has(name);
}

function byName(a, b) {
    return a.name < b.name ? -1 : a.name > b.name ? 1 : 0;
}

/** Recursively list a directory's files as `{ name, abs }` (sorted, posix names). */
export function sampleFiles(dir) {
    const out = [];
    const walk = (abs, rel) => {
        const entries = fs.readdirSync(abs, { withFileTypes: true }).sort(byName);
        for (const entry of entries) {
            if (isSkipped(entry.name)) continue;
            const childRel = rel ? `${rel}/${entry.name}` : entry.name;
            const childAbs = path.join(abs, entry.name);
            if (entry.isDirectory()) {
                walk(childAbs, childRel);
            } else if (entry.isFile()) {
                out.push({ name: childRel, abs: childAbs });
            }
        }
    };
    walk(resolveDir(dir), '');
    return out;
}

/** True when a sample directory has anything besides its `main.js` entry. */
export function hasResources(dir) {
    return sampleFiles(dir).some((f) => f.name !== 'main.js');
}

/** Read the curated manifest into `{ dir, id, title, category, description }`. */
export function readCurated() {
    const manifestPath = path.join(curatedDir, 'manifest.json');
    const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    return manifest.map((entry) => ({
        dir: entry.dir,
        id: entry.id || `curated:${entry.dir}`,
        title: entry.title,
        category: entry.category || 'Showcase',
        description: entry.description || '',
    }));
}

function resolveDir(dir) {
    return path.isAbsolute(dir) ? dir : path.join(curatedDir, dir);
}

/** Pack one sample directory with its contents at the archive root. */
export function packSample(dir) {
    const abs = resolveDir(dir);
    const files = sampleFiles(abs).map((f) => ({ name: f.name, data: fs.readFileSync(f.abs) }));
    return makeZip(files);
}

/** Pack every curated sample under a `<dir>/` prefix, plus optional extras. */
export function packSamples(entries, extras = []) {
    const files = [];
    for (const entry of entries) {
        const abs = resolveDir(entry.dir);
        for (const f of sampleFiles(abs)) {
            files.push({ name: `${entry.dir}/${f.name}`, data: fs.readFileSync(f.abs) });
        }
    }
    for (const extra of extras) {
        files.push({ name: extra.name, data: extra.data });
    }
    return makeZip(files);
}
