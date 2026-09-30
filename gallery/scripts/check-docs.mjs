#!/usr/bin/env node
// Regenerates the committed Markdown API reference into a temporary directory
// and fails if it differs from docs/api/. This keeps the checked-in reference
// from drifting from its source of truth, gallery/src/api/efx.d.ts.
//
// Same pattern as tools/gen_prelude.py --check: the generated output is
// committed, and a check mode proves it is current.
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import url from 'node:url';

const here = path.dirname(url.fileURLToPath(import.meta.url));
const galleryDir = path.resolve(here, '..');
const repoRoot = path.resolve(galleryDir, '..');
const committedDir = path.join(repoRoot, 'docs', 'api');

const isWindows = process.platform === 'win32';
const typedocBin = path.join(
    galleryDir,
    'node_modules',
    '.bin',
    isWindows ? 'typedoc.cmd' : 'typedoc',
);

function listFiles(root) {
    const out = [];
    const walk = (dir, prefix) => {
        for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
            const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
            if (entry.isDirectory()) {
                walk(path.join(dir, entry.name), rel);
            } else {
                out.push(rel);
            }
        }
    };
    if (fs.existsSync(root)) {
        walk(root, '');
    }
    return out.sort();
}

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'efx-docs-check-'));
let failed = false;

try {
    execFileSync(
        typedocBin,
        ['--options', 'typedoc.markdown.json', '--out', tmpDir],
        { cwd: galleryDir, stdio: 'inherit' },
    );

    const committed = listFiles(committedDir);
    const generated = listFiles(tmpDir);
    const committedSet = new Set(committed);
    const generatedSet = new Set(generated);
    const problems = [];

    for (const file of generated) {
        if (!committedSet.has(file)) {
            problems.push(`missing from docs/api/: ${file}`);
        }
    }
    for (const file of committed) {
        if (!generatedSet.has(file)) {
            problems.push(`stale file in docs/api/: ${file}`);
        }
    }
    for (const file of committed) {
        if (!generatedSet.has(file)) {
            continue;
        }
        const a = fs.readFileSync(path.join(committedDir, file));
        const b = fs.readFileSync(path.join(tmpDir, file));
        if (!a.equals(b)) {
            problems.push(`content differs: docs/api/${file}`);
        }
    }

    if (problems.length) {
        console.error('docs:check failed — docs/api/ is out of date:');
        for (const problem of problems) {
            console.error(`  ${problem}`);
        }
        console.error(
            '\nRun `npm --prefix gallery run docs:markdown` and commit docs/api/.',
        );
        failed = true;
    } else {
        console.log(`docs:check: docs/api/ is up to date (${committed.length} files)`);
    }
} finally {
    fs.rmSync(tmpDir, { recursive: true, force: true });
}

process.exit(failed ? 1 : 0);
