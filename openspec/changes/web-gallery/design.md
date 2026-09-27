# Design

## Context

See `proposal.md` — Why. The web player's boot is fixed: the Emscripten glue
runs `main()`, `postRun` fires synchronously, `entry.js`'s `__efxBoot()`
builds the `efx` API, reads `<root>/main.js` from MEMFS, evaluates it under a
host-global-shadowing sandbox, wires hooks, and starts the sokol frame loop
(`src/web/entry.js`, `src/web/bridge.c`). There is no runtime script-swap or
reset API, and the Pages workflow currently copies the raw `player_web.*`
output set (`player_web.html` → `index.html` plus JS/wasm/data).

Constraints that shape the approach:

- The engine's "no browser/Node dependencies" rule binds **game scripts**,
  not the host page or engine internals (`src/platform/platform.c` already
  reads `document.getElementById('canvas')` in capture mode). The gallery
  shell is host code and may use Svelte/TS/Monaco freely.
- Only the page's own native JS engine drives the web core (ADR 0022); the
  gallery adds no interpreter.
- The four-target gate and golden images are untouched by this change.

## Goals / Non-Goals

**Goals:**

- One engine instance per sample run, with state isolation and deterministic
  WebGL-context release.
- Edited source reaches a fresh engine run through a deliberately tiny,
  host-only engine hook.
- The sample catalog stays in lockstep with `tests/goldens/` automatically.
- A deployable static site whose only runtime network dependency is the
  Monaco CDN (which degrades gracefully).

**Non-Goals:**

- No script-visible API, no `docs/js-api.md` delta, no new unmanaged resource
  type or script-facing handle.
- No engine teardown/reset API, no in-place script swap.
- No change to the gate workflow, goldens, or the desktop player.

## Decisions

### D1 — iframe-per-run, one engine instance per run

Each sample/Run executes in a fresh iframe loading `player_web.js`. Switching
samples or re-running replaces the iframe.

*Why:* full state isolation for free, and navigating the iframe away releases
its WebGL context. Browsers cap live WebGL contexts (~16) and there is no
clean JS "destroy module" that releases one; reloading a document does.
*Rejected:*
- **MODULARIZE + one long-lived page instance** — re-instantiating the module
  in one document leaks rendering contexts across runs; needs `-sMODULARIZE`
  plus runtime-method exports and still has no context-release story.
- **Live eval/reset bridge API in one instance** — instant edits, but requires
  tearing down hooks, the display list, and every GC-finalized GPU handle
  between scripts; not a gallery-sized problem and risky against ADR 0011/0012
  resource ownership.

### D2 — Host-provided entry source (`globalThis.__efx_main_js`)

Before evaluating the entry script, `entry.js` checks for a host-supplied
source on the global object; if present it uses that string instead of
reading `<root>/main.js`, and deletes the global before evaluation so the
script (which can reach a `globalThis` proxy) cannot observe it. When absent,
behavior is exactly as today.

*Why:* the only injection point is before `player_web.js` loads (boot is
synchronous), the iframe controls that timing, and the hook adds no
script-visible API.
*Rejected:*
- **Normal-mode `?root=` + `FS.writeFile` of edited code** — needs
  `-sMODULARIZE`/`-sEXPORTED_RUNTIME_METHODS` to reach FS before `main()`, and
  writing edited code to a virtual root is heavier than passing a string.
- **Post-boot injection** — the frame loop already started; there is nothing
  to swap into without the D1-rejected reset API.

### D3 — Explicit Run/Reset, not keystroke-live

The editor executes on an explicit control, with Reset restoring the
original source. An optional debounced re-run may be offered.

*Why:* explicit runs keep the iframe-per-run cost (~100–300 ms wasm
re-instantiation, cached bytes) off the typing path and match the
"sample as an application" model. Keystroke-live would require the
rejected engine reset API.

### D4 — Runner handshake over `postMessage`

The runner page (`runner.html`) waits for a `{ code }` message from the host,
sets `globalThis.__efx_main_js`, then injects `<script src="player_web.js">`.
The host validates `event.origin`/`event.source` (same origin).

*Why:* deterministic — the module never boots before the code arrives —
and avoids URL-length limits on sample source.
*Rejected:* **setting a global on `contentWindow` before load** (racy against
`postRun`) and **`?code=` base64** (URL size limits, encoding fragility).

### D5 — Sample catalog generated from goldens + curated set

A build-time Node script scans `tests/goldens/*/main.js` (sorted by name) and
emits the catalog, merged with hand-written curated showcases carrying
metadata (title, category, description). The generated file is not edited by
hand.

*Why:* makes the existing "the gallery mirrors the goldens" invariant
automatic and removes the current manual mirror in `examples/browser/main.js`.
*Rejected:* **hand-maintained manifest** (drifts) and **goldens-only** (terse
fixtures teach poorly; the hybrid keeps teaching samples without a second
source of truth — the goldens still drive the generated half).

### D6 — Vite + TypeScript + Svelte; Monaco from CDN

The host app is Vite + TS + Svelte; the editor is Monaco loaded lazily from a
pinned CDN. Vite `base` is relative (`./`) so the site works under a project
Pages path (`/<repo>/`).

*Why:* small host bundle, first-class TS for `efx.d.ts`, trivial state for the
list/canvas/editor. CDN keeps Monaco out of the bundle and the Pages artifact.
*Rejected:* **React** (heavier than needed for three panels); **bundling
Monaco** (multi-MB in the artifact and cache churn); **absolute Vite base**
(breaks project-page subpaths).

### D7 — `efx.d.ts` is a maintained living document

`efx.d.ts` is hand-authored to describe current API behavior and is updated in
the same change as any API/`docs/js-api.md` change. It is surfaced to Monaco
via `addExtraLib`. `AGENTS.md`'s doc map gains it beside `docs/js-api.md`.

*Why:* the API is defined in C plus the JS prelude with no machine-readable
manifest; a generated declaration would need one first. A maintained document
is the honest source today.
*Rejected:* **generating from `docs/js-api.md`** (prose is not a manifest yet;
can be revisited without changing behavior); **deferring types** (the editor
experience is the point).

### D8 — Pages builds the wasm player plus the gallery; gate untouched

`.github/workflows/pages.yml` keeps its triggers/gating/pinned emsdk, builds
`player_web`, copies `player_web.{js,wasm,data}` into the gallery bundle,
builds the gallery with Node/Vite, and deploys the gallery `dist/`. The gate
workflow is unchanged.

*Why:* separates deploy from verification (ADR 0023 lineage) and keeps the
downloadable web bundle artifact (which still packages `player_web.*`)
consistent.

### D9 — Reuse the existing `player_web` build; only add the entry hook

The gallery embeds the unmodified global-`Module` `player_web.js`/`.wasm`/
`.data` (preloaded `examples/browser` root included but bypassed by D2). The
runner page is gallery-owned HTML, not Emscripten's generated shell.

*Why:* smallest engine/build delta, preserves the existing output set the
verification spec pins, and avoids `-sMODULARIZE` churn.
*Rejected:* **a new MODULARIZE gallery target** (extra build shape and
context lifecycle work for no behavioral gain).

### D10 — App area constrained to 4:3

The sample canvas is displayed in a 4:3 area so the 640×480 sample frame
stretches without distortion. No engine surface-size override is added.

*Why:* sokol stretches the 2D frame to the surface (`platform.c` uses the
window size); matching the panel aspect to the sample frame keeps samples
composed as authored without an engine change.

### D11 — Site verification is a headless boot smoke, not a gate job

A dev tool serves the built bundle in pinned headless Chrome, runs a sample,
and asserts the page boots and the canvas renders without console errors
(mirroring `tools/run_web_goldens.mjs`). The ctest/golden gate is unchanged.

*Why:* ctest cannot assert HTML; the Pages run plus a local headless smoke is
the honest signal for a site. Keeps the four-target gate's meaning intact.

**Resource exposure:** this change exposes no new unmanaged resource to
scripts and adds no script-visible handle — the host channel (D2) is not
reachable by scripts, and the gallery's own resources are host-page objects.

## Risks / Trade-offs

- **WebGL context exhaustion across many runs** → iframe-per-run (D1) frees
  each context on navigation; the D11 smoke runs many samples in sequence and
  fails if contexts accumulate.
- **Monaco CDN unavailable/offline** → the editor degrades to a plain text
  editing surface and the sample still runs; CDN version is pinned.
- **Player asset paths inside the iframe** → reference `player_web.js` by a
  stable relative path from `runner.html`; verify under the Pages subpath in
  the smoke and via D6's relative base.
- **Golden scripts as samples** → they are self-contained and already run
  standalone (the golden harness runs each `main.js`), but they set their own
  cameras/frames; the 4:3 panel (D10) keeps them composed correctly.
- **Manifest ordering nondeterminism** → generate sorted by scene name so
  builds are stable and diffs are reviewable.
- **Retiring `examples/browser/main.js`** → it is named in `CMakeLists.txt`
  `LINK_DEPENDS` and in `AGENTS.md`; both are updated in the same change.
- **Pages subpath** → relative `base` (D6) avoids the class of 404s that
  project pages otherwise cause.

## Migration Plan

1. Land the engine hook (D2) with the existing Pages/bundle behavior intact —
   absent an override, nothing changes.
2. Add the gallery, generated catalog, `efx.d.ts`, and runner; wire the Pages
   workflow to build and deploy the gallery `dist/`.
3. Merge to `main`; the Pages run deploys the gallery. Rollback: revert the
   `pages.yml` collect/build steps to the previous `player_web.*` copy (the
   target is still built).

## Open Questions

- The exact curated showcase scripts and the final PS2 palette/typography
  tokens (presentation only; the shell contract is fixed).
- Whether `efx.d.ts` later gains a generated component once an API manifest
  exists (no behavior or approach change).
- The pinned Monaco CDN version.
