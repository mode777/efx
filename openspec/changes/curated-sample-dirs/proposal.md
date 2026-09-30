# Proposal

## Why

Curated gallery samples keep their script, assets, and mountable pack in
three independent places: a flat `<name>.js` at `gallery/samples/curated/`,
a committed `<name>.zip` beside it, and (for `audio-showcase` /
`modules-showcase`) a separate readable source directory plus a bespoke
Python packer. Every asset sample therefore has to be maintained twice, and
the release has no downloadable sample pack at all — a visitor who grabs a
player binary has no matching sample set to run.

## What Changes

- **Curated samples become self-contained directories.** Each curated sample
  moves to `gallery/samples/curated/<name>/` holding `main.js` plus its
  assets/modules, so the directory *is* the player's resource root
  (`player <name>`). Asset-less samples are just `<name>/main.js`.
- **The curated manifest drops `file`/`assets` in favour of `dir`.** The
  catalog id derives from the directory name; title/category/description stay.
- **The gallery build derives each sample's mountable pack from its
  directory.** `gen-catalog.mjs` reads `<dir>/main.js` and, when the
  directory has files besides `main.js`, packs the directory contents at the
  archive root into `public/samples/<name>.zip` (gitignored, as today).
  Asset-less samples keep `assets: null`.
- **A new release archive `emotion-fx-<EFX_VERSION>-samples.zip`** is built
  from the same directories: each `<name>/**` under a `<name>/` prefix, so a
  visitor extracts it and runs `player <name>` per sample.
- **One deterministic packer shared by both consumers.** A tiny pure-Node
  zip writer (stored entries, fixed 1980 timestamp, own CRC32) plus a
  samples helper back the gallery build and the release packer.
- **The three synthesized effect WAVs stay reproducible.** A renamed
  generator emits the committed loose WAVs inside `audio-showcase/` and a
  `--check` mode fails CI on drift.
- **Retire** `pack-curated-audio.py`, `pack-curated-modules.py`, the
  committed curated `*.zip` files, and the flat curated `*.js` files.
- **CI gains a `samples` job** that builds and uploads the pack as a
  workflow artifact on every gate run; the `release` job downloads it and
  attaches it to the GitHub Release alongside the four platform archives.

## Capabilities

### New Capabilities

_None._ This is a build/distribution and sample-authoring refactor; it
changes existing capability requirements rather than introducing a new
capability.

### Modified Capabilities

- `web-gallery`: the "Sample asset packs" requirement changes — a curated
  sample is a self-contained resource-root directory, the build derives the
  mountable pack from it, and the directory is runnable by the player as-is
  (no separately committed pack). The catalog/manifest contract is stated in
  terms of the sample directory.
- `verification`: the "Downloadable per-target build artifacts" requirement
  gains a fifth archive — the curated-samples pack — published as a workflow
  artifact on every gate run and attached to the release on tag runs.

## Milestone

This change implements **no roadmap milestone (F1–F14)**. It is post-F14
build/release tooling and sample-packaging work that follows the completed
ladder; it makes no `feature-roadmap` delta and does not change engine
behavior. The config rule to name a milestone is satisfied by this explicit
statement.

## Docs and decisions

- **New ADR `docs/decisions/0043-curated-sample-dirs.md`** (added to the
  `docs/decisions/README.md` index): the durable invariant that a curated
  sample is a self-contained resource-root directory and both the gallery
  mount pack and the release sample pack are derived from it by one
  deterministic packer. This outlives the change because future samples must
  follow the layout.
- `gallery/samples/curated/CREDITS.md`: packs → directories; recipes retarget
  to the sample folder.
- `AGENTS.md`: the current-state paragraph that describes committed curated
  asset packs.
- **No** `docs/js-api.md` or `docs/api/` change — there is no script-facing
  API change.

## Non-goals

- **Golden-scene asset packs.** `tests/goldens/<scene>/assets.zip` duplicates
  loose golden assets in the same way, but goldens are not part of the
  release sample pack; unifying them is out of scope.
- **The web resource provider.** The engine still mounts exactly one
  host-provided zip (F6a/ADR 0031); the per-sample pack remains a zip, now
  derived rather than hand-committed.
- **No engine, runtime, binding, or script-API change.** No new `efx` symbol,
  no new resource type.
- **No gallery UI change.** The catalog shape the Svelte app consumes
  (`source`, optional `assets`) is unchanged; only how it is produced changes.
- **No new third-party dependency** — the packer is hand-written Node.

## Impact

- `gallery/samples/curated/**` layout, `manifest.json`, `CREDITS.md`
- `gallery/scripts/` (new `lib/zip.mjs`, `lib/samples.mjs`,
  `pack-samples.mjs`, renamed audio generator; `gen-catalog.mjs`; deleted
  Python packers)
- `gallery/public/samples/` (still generated/ignored)
- `.github/workflows/ci.yml` (`samples` job; `release` job download/attach)
- `openspec/specs/web-gallery`, `openspec/specs/verification`
- `docs/decisions/` (new ADR + index), `AGENTS.md`
