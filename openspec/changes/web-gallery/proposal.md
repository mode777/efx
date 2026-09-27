# Proposal

**Roadmap position:** Cross-cutting showcase/tooling infrastructure — no
feature milestone. It adds the public sample gallery that presents the
already-shipped F1–F5 API surface and the Pages deployment that carries it;
it does not start or reopen a ladder milestone. Precedent: the archived
`separate-pages-deploy` change ("Cross-cutting CI infrastructure — no
feature milestone").

## Why

The public Pages demo is a single script (`examples/browser/main.js`) that
auto-cycles every golden scene with no navigation, no source view, and no
way to try the API. It advertises the engine poorly: a visitor cannot pick
a sample, read its code, or change it. We want a three.js-style sample site
— samples listed on the left, the running app on the upper right, and an
editable JavaScript panel below it — so the shipped API is both showable
and hands-on.

## What Changes

- **New web gallery app** (Vite + TypeScript + Svelte) hosted on GitHub
  Pages: a left sample list, a live canvas that runs the selected sample in
  an isolated iframe, and a bottom-right Monaco editor (loaded from a CDN)
  with `efx.d.ts` type definitions, a Run/Reset control, and inline error
  reporting. A PlayStation-2-era visual language (dark navy, teal glow,
  beveled panels) styles the shell.
- **New engine embedding hook**: the web runtime (`src/web/entry.js`)
  prefers a host-provided entry source (`globalThis.__efx_main_js`) over the
  resource-root `main.js`, consuming and deleting it before evaluation so
  game scripts can never observe it. This lets the host run one fresh
  engine instance per sample/Run in an iframe, which isolates sample state
  and releases the WebGL context on navigation.
- **Generated sample catalog**: a build step scans `tests/goldens/*/main.js`
  and emits the sample manifest (keeping the "gallery mirrors the goldens"
  invariant automatic), merged with a small curated showcase set.
- **`efx.d.ts` as a living API document**: a hand-maintained TypeScript
  declaration of the public `efx` surface that grows in lockstep with the
  API and `docs/js-api.md`; pointed to from `AGENTS.md` so every API change
  updates it in the same change.
- **Pages workflow rebuild**: `.github/workflows/pages.yml` builds the
  Emscripten player and the gallery bundle (Node/Vite) and deploys the
  gallery's static `dist/` instead of the raw `player_web.*` file set.
- **BREAKING (deployment layout)**: the files served at the GitHub Pages
  root change from `player_web.html`/`.js`/`.wasm`/`.data` to the gallery's
  built bundle; any external links to the old `player_web.*` paths break.
- Retire `examples/browser/main.js` as the Pages app. The golden-scene
  source of truth remains `tests/goldens/*/main.js`.

## Capabilities

### New Capabilities

- `web-gallery`: the public sample-gallery site — sample catalog and
  selection, isolated per-run engine hosting, the editable-code run loop,
  the API type document surfaced to the editor, and the Pages build/deploy
  of the site.

### Modified Capabilities

- `player-runtime`: adds a host-provided entry source to the web entry-script
  contract (the web player uses a host-supplied script when present, and the
  channel stays invisible to game scripts).
- `verification`: the Pages deployment requirement changes from deploying the
  raw `player_web.*` output set to building the Emscripten player plus the
  gallery bundle and deploying the gallery's static output.

## Impact

- **Source / engine:** `src/web/entry.js` (host entry-source override);
  `src/platform/web_pre.js` and the `player_web` CMake wiring as needed to
  serve the gallery runner.
- **New app:** a gallery project (Vite + TypeScript + Svelte), its generated
  sample manifest, `efx.d.ts`, the runner page, and the PS2 design tokens.
- **Build / CI:** `.github/workflows/pages.yml` (Node + Vite step, deploy the
  gallery `dist/`); `package.json`/lockfile for the gallery toolchain.
- **Docs:** new ADR `docs/decisions/0030-...` for the host↔engine embedding
  contract; `AGENTS.md` doc map and current-state/verification sections gain
  the gallery and `efx.d.ts`; `docs/js-api.md` — **no delta** (no
  script-visible API change), stated here so the same-change rule is not
  overlooked.
- **Tests:** no `ctest`/golden change; the gallery build is validated by its
  own build and a headless boot smoke (design decides the exact check).

## Non-goals

- **No script-visible API change** and no `docs/js-api.md` delta; the new
  host channel is not exposed to game scripts.
- **No live keystroke execution / no runtime script-swap engine API.** Runs
  happen on an explicit control (and optionally a debounced re-run); there is
  no `eval`/reset bridge API and no engine teardown work.
- **No change to the four-target gate**, golden images, tolerance, or
  determinism (ADR 0020); the gallery is not part of the milestone gate.
- **No new samples that exercise unimplemented API** (F6+); the catalog is
  built from shipped features only.
- **No change to `tests/goldens/`** as the verification source of truth.
- **No replacement of the desktop player or its `--script` mode.**
