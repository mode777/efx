#!/usr/bin/env node
// Generates the gallery catalog from the committed golden scenes plus the
// curated showcase set. Deterministic: entries are sorted by id and the
// output shape is stable, so the generated file is a pure function of the
// repository's tests/goldens/ + samples/curated/ content.
//
// Golden scenes are the raw material (see design D5); the curated manifest
// adds teaching samples. This keeps the "gallery mirrors the goldens"
// invariant automatic: a new golden scene appears in the catalog without a
// manual gallery edit.
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const here = path.dirname(url.fileURLToPath(import.meta.url));
const galleryDir = path.resolve(here, '..');
const repoRoot = path.resolve(galleryDir, '..');
const goldensDir = path.join(repoRoot, 'tests', 'goldens');
const curatedDir = path.join(galleryDir, 'samples', 'curated');
const outFile = path.join(galleryDir, 'src', 'samples', 'generated.json');

function titleCase(s) {
    return s.replace(/[_-]+/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());
}

/** Category derived from the golden scene name convention. */
function categoryFor(name) {
    if (name.startsWith('light_')) return 'Lighting';
    if (name.startsWith('map_')) return 'Materials';
    if (name.startsWith('post_')) return 'Post FX';
    if (name.startsWith('rt_')) return 'Render Targets';
    if (name.endsWith('3d')) return '3D';
    return '2D';
}

/** First block of leading `//` comment lines, joined as the description. */
function leadingComment(source) {
    const lines = source.split('\n');
    const out = [];
    for (const line of lines) {
        const m = line.match(/^\s*\/\/\s?(.*)$/);
        if (!m) break;
        out.push(m[1].trim());
    }
    return out.join(' ').trim();
}

function goldenSamples() {
    if (!fs.existsSync(goldensDir)) return [];
    const names = fs
        .readdirSync(goldensDir, { withFileTypes: true })
        .filter((d) => d.isDirectory())
        .map((d) => d.name)
        .filter((n) => fs.existsSync(path.join(goldensDir, n, 'main.js')))
        .sort();
    const samplesDir = path.join(galleryDir, 'public', 'samples');
    return names.map((name) => {
        const source = fs.readFileSync(path.join(goldensDir, name, 'main.js'), 'utf8');
        const sample = {
            id: `golden:${name}`,
            origin: 'golden',
            title: titleCase(name),
            category: categoryFor(name),
            description: leadingComment(source) || `Golden scene: ${name}`,
            source,
        };
        // Scenes with extra resource files ship a committed asset pack; copy
        // it into the site and point the runner's host asset channel at it.
        const assetZip = path.join(goldensDir, name, 'assets.zip');
        if (fs.existsSync(assetZip)) {
            fs.mkdirSync(samplesDir, { recursive: true });
            fs.copyFileSync(assetZip, path.join(samplesDir, `${name}.zip`));
            sample.assets = `samples/${name}.zip`;
        }
        return sample;
    });
}

function curatedSamples() {
    const manifestPath = path.join(curatedDir, 'manifest.json');
    if (!fs.existsSync(manifestPath)) return [];
    const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    return manifest.map((entry) => {
        const source = fs.readFileSync(path.join(curatedDir, entry.file), 'utf8');
        return {
            id: entry.id || `curated:${entry.file.replace(/\.js$/, '')}`,
            origin: 'curated',
            title: entry.title,
            category: entry.category || 'Showcase',
            description: entry.description || '',
            source,
        };
    });
}

const samples = [...goldenSamples(), ...curatedSamples()].sort((a, b) =>
    a.id < b.id ? -1 : a.id > b.id ? 1 : 0
);

const catalog = {
    generated: true,
    goldenCount: samples.filter((s) => s.origin === 'golden').length,
    curatedCount: samples.filter((s) => s.origin === 'curated').length,
    samples,
};

fs.mkdirSync(path.dirname(outFile), { recursive: true });
fs.writeFileSync(outFile, JSON.stringify(catalog, null, 2) + '\n');
console.log(
    `gen-catalog: ${catalog.goldenCount} golden + ${catalog.curatedCount} curated -> ${path.relative(repoRoot, outFile)}`
);
