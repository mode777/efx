# 0044 — A curated gallery sample is a self-contained resource-root directory; its gallery and release packs are derived

Status: Accepted (2026-09, change `curated-sample-dirs`)

## Context

Curated showcase samples were spread across three overlapping sources: a flat
`<name>.js` script, a committed `<name>.zip` beside it, and — for
`audio-showcase`/`modules-showcase` — a separate readable source directory
plus a bespoke Python packer. Every asset sample had to be maintained twice,
and the release shipped no sample set at all. The gallery's web runner mounts
exactly one host-provided zip as a sample's resource root (ADR 0031), so the
gallery needs a zip per asset sample, while the player takes a directory or a
zip. Full process record: `openspec/changes/curated-sample-dirs/`.

## Decision

- **The curated sample directory is the single source of truth.** Each sample
  lives at `gallery/samples/curated/<name>/` containing `main.js` and every
  resource it loads, so the directory is itself the player's resource root
  (`player <name>`). `manifest.json` entries carry `dir`; the catalog id
  defaults to `curated:<dir>`.
- **One deterministic pure-Node packer derives both archives.**
  `gallery/scripts/lib/zip.mjs` writes stored (uncompressed) entries with a
  fixed 1980-01-01 timestamp and a hand-rolled CRC32; `lib/samples.mjs`
  exposes `packSample(dir)` (contents at the archive root) and
  `packSamples(entries)` (each `<name>/**` under a `<name>/` prefix). The
  gallery build writes `public/samples/<name>.zip` from `packSample` only when
  the directory holds files besides `main.js`; the release packer
  (`gallery/scripts/pack-samples.mjs`) writes
  `efx-<EFX_VERSION>-samples.zip` from `packSamples` plus a root
  `CREDITS.md`.
- **Generated assets are committed loose and drift-checked.**
  `gallery/scripts/gen-audio-assets.py` writes the three synthesized effect
  WAVs into `audio-showcase/` and `--check` fails on drift; the build never
  mutates authored sources.
- **No separately committed curated packs.** The per-sample zips are derived
  and gitignored; a CI `samples` job builds the release archive on every gate
  run and the `release` job attaches it.

## Consequences

- Adding a sample means creating a directory, not a script-plus-zip pair;
  adding resources to a sample changes its derived pack automatically. Future
  sample work must follow this layout rather than reintroducing a committed
  pack.
- The derived gallery pack now contains `main.js`. The runner's host-only
  `__efx_main_js` override (ADR 0030) makes it inert there, while it keeps the
  pack a complete, standalone player resource root.
- Every gate run publishes a fifth downloadable archive; a tag run attaches
  the curated-samples pack to the GitHub Release alongside the four platform
  archives, so a visitor who downloads a player binary gets a matching,
  runnable sample set.
- Archives are byte-reproducible across hosts and Node versions, so a rebuild
  produces no diff.

## Rejected alternatives

- **Keep flat scripts plus committed zips.** This is the duplication the
  change removes; three sources of truth decay independently.
- **Keep a Python `zipfile` packer.** It would make the Node gallery build
  depend on a Python step and split one packer across two languages.
- **Add an npm zip dependency (fflate/jszip).** A new dependency whose
  compression output must be pinned for determinism, for ~80 lines of code;
  stored entries make determinism trivial.
- **Deflate-compress the archives.** Node's deflate output is not stable
  across versions, and the packs are small (≈1.2 MB total).
- **Synthesize the WAVs at build time.** A build that writes into the source
  tree dirties the checkout and makes the archive depend on build order.
- **Build `samples.zip` only inside the release job.** It would not publish a
  downloadable artifact on manual gate runs, breaking the "every run
  publishes archives" symmetry.
