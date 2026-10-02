# Design

## Context

See `proposal.md` — Why. The relevant current machinery:

- `gallery/scripts/gen-catalog.mjs` reads `manifest.json`, inlines each
  curated `entry.file` as the catalog `source`, and copies `entry.assets`
  (a committed zip) to `public/samples/` for the runner to mount.
- `gallery/public/runner.html` boots the player with `__efx_main_js` (the
  inline source) and, when present, `__efx_assets` — a single host-provided
  zip the engine fetches and mounts as the resource root (F6a / ADR 0031).
  The web resource provider is zip-only; there is no directory mount on web.
- `gallery/scripts/pack-curated-audio.py` and `pack-curated-modules.py`
  generate two of the committed packs from separate source directories.
- `.github/workflows/ci.yml` builds four platform archives in the `native`
  and `emscripten` jobs; the `release` job (tag-gated) downloads `player-*`
  artifacts and attaches them.

No engine, binding, or script-API code is involved. The web entry path is
always `main.js` at the resource root; `__efx_main_js` overrides a mounted
`main.js`, so a pack that also contains `main.js` is inert to the gallery
runner but valid as a standalone player root.

## Goals / Non-Goals

**Goals:**

- One authored source per curated sample: a directory that is simultaneously
  the player resource root and the input to both derived archives.
- Byte-reproducible derived archives, so a rebuild produces no diff.
- The release gains a player-runnable curated-samples pack, built by the
  same code path the gallery uses.

**Non-Goals:**

- Changing the web resource provider (still one zip).
- Deduplicating golden-scene `assets.zip` (see proposal Non-goals).
- Any change to the catalog shape the Svelte app consumes (`source`,
  optional `assets` stay).

## Decisions

### D1 — Curated sample is a self-contained directory

Each sample lives at `gallery/samples/curated/<name>/` with `main.js` and
its resources; `manifest.json` entries carry `dir` (id defaults to
`curated:<dir>`) instead of `file`/`assets`. The directory is the single
source of truth.

- *Alternative — keep flat scripts and a separate committed zip:* rejected;
  that is exactly the duplication this change removes.
- *Alternative — a `samples/<name>.json` descriptor instead of a manifest
  array:* rejected; the existing manifest already carries title/category/
  description and needs only the field swap.

### D2 — One pure-Node deterministic packer, two layouts

`gallery/scripts/lib/zip.mjs` writes a zip as stored (uncompressed) entries
with a fixed 1980 timestamp and a hand-rolled CRC32; `lib/samples.mjs`
exposes `packSample(dir)` (contents at the archive root) and
`packSamples(entries)` (each `<name>/**` under a `<name>/` prefix). The
gallery build calls `packSample`; the release packer calls `packSamples`.

- *Alternative — Python `zipfile` as today:* rejected; it would make the
  Node gallery build depend on a Python step and split the packer across
  languages.
- *Alternative — an npm zip dependency (fflate/jszip):* rejected; adds a
  dependency whose compression output must be pinned for determinism, for
  ~80 lines of code. Stored entries make determinism trivial.
- *Alternative — deflate:* rejected; Node's deflate output is not stable
  across versions, and the packs are small (≈1.3 MB total).

### D3 — The derived gallery pack includes `main.js`

When a curated directory has files besides `main.js`, `packSample` packs the
whole directory including `main.js`. The gallery runner still passes the
inline `source`, so the mounted `main.js` is overridden and inert, while the
pack remains a complete, downloadable player resource root. Asset-less
directories produce no pack (`assets: null`), preserving today's runtime
behavior and avoiding a needless fetch.

- *Alternative — exclude `main.js` from the gallery pack:* rejected; it
  buys a few KB but adds a second packing rule and makes the pack
  non-self-contained.

### D4 — Synthesized WAVs are committed loose, with a drift check

The three effect WAVs become committed files inside `audio-showcase/`. A
renamed generator (`gallery/scripts/gen-audio-assets.py`) writes them into
the sample directory and supports `--check` to fail on drift, mirroring the
golden-image bootstrap discipline. The build never mutates authored sources.

- *Alternative — synthesize at build time:* rejected; a build that writes
  into the source tree makes the checkout dirty and the archive depend on
  build order.
- *Alternative — drop the generator and commit opaque WAVs:* rejected;
  losing the reproducible recipe is a regression in transparency.

### D5 — A dedicated `samples` CI job

`ci.yml` gains a `samples` job (checkout + `node gallery/scripts/
pack-samples.mjs` + upload artifact `samples`) that runs on every gate run.
The `release` job adds it to `needs` and downloads the `samples` artifact
alongside `player-*`, then attaches everything with the existing
`gh release create/upload`.

- *Alternative — build the pack only inside the release job:* rejected; it
  would not publish a downloadable artifact on manual gate runs, breaking
  the "every run publishes archives" symmetry.

### D6 — Spec and decision records

`web-gallery`'s "Sample asset packs" requirement is rewritten to state the
directory-derived pack; `verification`'s "Downloadable per-target build
artifacts" requirement gains the curated-samples archive. A new ADR
`docs/decisions/0044-curated-sample-dirs.md` records the durable authoring/
distribution invariant (0043 was already taken by
`web-pointer-focus-default`). No new capability: the behavior belongs to the
existing gallery and verification capabilities.

### D7 — Release archive name

`emotion-fx-<EFX_VERSION>-samples.zip`, matching the
`emotion-fx-<EFX_VERSION>-<os>-<arch>` family. The pack also carries a copy
of `CREDITS.md` at its root for provenance (CC0 requires none; this is for
transparency).

## Risks / Trade-offs

- [The packer's stored entries inflate the release zip] → ~1.3 MB
  uncompressed is acceptable; determinism is worth more than compression for
  this artifact.
- [A sample directory accumulates stray files (editor backups, `golden.png`)
  that leak into the pack] → `packSample`/`packSamples` skip dotfiles and
  known non-resource names; the review task checks each directory's
  contents.
- [Moving `audio/` and `modules/` into their sample directories breaks a
  stale reference in CREDITS.md or a pack script] → the change deletes the
  pack scripts and rewrites CREDITS.md in the same step; a repo-wide search
  for the old paths is part of the tasks.
- [The gallery pack now contains `main.js`, and a future provider change
  starts reading it] → the engine's `__efx_main_js` override is part of the
  embedding contract (ADR 0030); the pack is byte-identical to a valid
  player root, so either path runs the same code.

## Migration Plan

Pure repository refactor; no runtime or data migration. Land as one change:
restructure directories, swap the packer, update CI, specs, ADR, and docs.
Rollback is a revert; the previously committed zips remain in history. The
next `v*` tag after landing will attach the new archive.

## Open Questions

None. The one deferrable item — extending the directory-derived pack to
golden scenes — is explicitly a non-goal and can be proposed later without
changing this design.
