# Tasks

## 1. Restructure curated samples into self-contained directories

- [x] 1.1 For every curated sample, create `gallery/samples/curated/<name>/` and move the flat `<name>.js` to `<name>/main.js`; verify with `ls gallery/samples/curated` that no `*.js` remains at the curated root.
- [x] 1.2 Extract each committed curated `<name>.zip` into its `<name>/` directory (assets at the directory root) and delete the zip; verify with `unzip -l` before deletion and `find` after that each sample's resources sit beside its `main.js`.
- [x] 1.3 Fold `gallery/samples/curated/audio/` into `audio-showcase/` and `gallery/samples/curated/modules/` into `modules-showcase/`, then delete the two source directories; verify `audio-showcase/` holds `music.mp3`, `font.ttf` and `modules-showcase/` holds `lib/` and `data/` beside `main.js`.
- [x] 1.4 Change `manifest.json` entries from `file`/`assets` to `dir` (id defaults to `curated:<dir>`); verify `npm --prefix gallery run gen:catalog` still emits one catalog entry per curated sample.
- [x] 1.5 Search the repository for the removed flat paths (`<name>.js`, curated `*.zip`, `curated/audio`, `curated/modules`) and update `CREDITS.md` recipes to point at the sample directories; verify no stale reference remains via `rg`. (Also updated the desktop gallery smoke tests in `tests/CMakeLists.txt` to use the sample directories.)

## 2. Deterministic pure-Node packer

- [x] 2.1 Implement `gallery/scripts/lib/zip.mjs` — stored entries, fixed 1980-01-01 timestamps, own CRC32 — and verify a round-trip through `unzip -t` on a fixture.
- [x] 2.2 Implement `gallery/scripts/lib/samples.mjs` (`readCurated`, `packSample(dir)` contents-at-root, `packSamples(entries)` `<name>/` prefix, skipping dotfiles and non-resource names); verify each helper against the restructured samples.
- [x] 2.3 Verify determinism: build the same pack twice and assert the two buffers are byte-identical.

## 3. Gallery build derives packs

- [x] 3.1 Update `gen-catalog.mjs` to read `<dir>/main.js` as `source` and, only when the directory has files besides `main.js`, write `public/samples/<name>.zip` via `packSample`; verify `generated.json` has `assets` for asset-bearing samples and none for asset-less ones.
- [x] 3.2 Remove the committed-zip copy logic and the `entry.assets` path from `gen-catalog.mjs`; verify `npm --prefix gallery run build` succeeds and `public/samples/` contains exactly the derived packs.
- [x] 3.3 Verify the built gallery in a browser with `node tools/run_gallery_smoke.mjs` (all checks pass, no console/page errors). (Run via `tools/verify_remote.py` on the verification server — the local container's Chrome lacks the X/glib symbols; the server gallery suite passed.)

## 4. Audio asset generation

- [x] 4.1 Rename `pack-curated-audio.py` to `gallery/scripts/gen-audio-assets.py`, retarget it to write the loose WAVs into `audio-showcase/`, and keep `--check`; verify `python3 gallery/scripts/gen-audio-assets.py --check` passes against the committed WAVs.
- [x] 4.2 Delete `gallery/scripts/pack-curated-modules.py` and the now-unused committed packs; verify no script or workflow references them via `rg`.

## 5. Release packer and CI

- [x] 5.1 Add `gallery/scripts/pack-samples.mjs` that writes `emotion-fx-<EFX_VERSION>-samples.zip` via `packSamples` plus a root `CREDITS.md`; verify locally with `node gallery/scripts/pack-samples.mjs --out /tmp/samples.zip` that each top-level `<name>/` holds `main.js` and its resources.
- [x] 5.2 Add a `samples` job to `.github/workflows/ci.yml` (checkout, run the packer, upload artifact `samples`); verify with a local `act`-free dry read that the job builds `dist/*.zip` and that the workflow YAML parses.
- [x] 5.3 Add `samples` to the `release` job's `needs`, download the `samples` artifact into `dist`, and confirm the existing `gh release create/upload` attaches it; verify by inspecting the rendered workflow diff.
- [x] 5.4 Verify player-runnability: extract a locally built `samples.zip` and run `player --script <name>/main.js --root <name>` for an asset-less sample and an asset-bearing sample, expecting exit 0. (No local player build; covered by the native `smoke_showcase_*` ctest cases, which now run each sample against its directory resource root and passed on the server.)

## 6. Specs, ADR, and docs

- [x] 6.1 Write `docs/decisions/0044-curated-sample-dirs.md` (TEMPLATE.md) recording the directory-is-source-of-truth invariant and derived archives, and add its row to `docs/decisions/README.md`; verify the index links the file. (0043 is taken by `web-pointer-focus-default`.)
- [x] 6.2 Update the `AGENTS.md` current-state section (and the gallery bullet) to describe curated sample directories and the release samples pack; verify the old "committed zip beside its manifest" wording is gone.
- [x] 6.3 Confirm no `docs/js-api.md` or `docs/api/` change is needed (no script-facing API delta) and state so in the change record. (Already stated in `proposal.md` — Docs and decisions.)

## 7. Integration verification

- [ ] 7.1 Run `npx openspec validate curated-sample-dirs --strict` and confirm the change validates.
- [x] 7.2 Verify on the SSH verification server (`python3 tools/verify_remote.py all <branch>`) that the gallery build and smoke remain green; fix and re-verify on failure.
- [ ] 7.3 Dispatch the gate (`gh workflow run ci.yml --ref <branch>`) and confirm the `samples` artifact exists; confirm a tag run attaches `emotion-fx-<EFX_VERSION>-samples.zip` to the release alongside the four platform archives.
