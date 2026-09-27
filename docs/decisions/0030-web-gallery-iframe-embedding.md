# 0030 — The sample gallery embeds the web player per-run in an iframe, fed by a host-provided entry source

Status: Accepted (2026-09, change `web-gallery`; amended 2026-09, change
`f6a-resource-loading`: the host may also supply an asset-root URL)

Supports: vision.md (public showcase of the consumer API); ADR 0008 (Node is
a test launcher only; game scripts have no browser/Node deps); ADR 0011/0012
(GC-finalized resources, destroy-first discipline); ADR 0016 (loading
`main.js` is the implicit init); ADR 0022 (the browser's native JS engine is
the web runtime, driven through `src/web/`); ADR 0023 (Pages deploys
separately from the gate).

## Context

The public Pages demo must let a visitor browse samples, run one in the
browser, edit its source, and run the edit. The web player's boot is fixed:
the Emscripten glue runs `main()`, `postRun` fires synchronously, `entry.js`
reads `<root>/main.js` from MEMFS, evaluates it under a host-global-shadowing
sandbox, and starts the sokol frame loop. There is no runtime API to swap or
reset a script, and no way to hand the engine new source after boot. The full
process record is `openspec/changes/web-gallery/`.

## Decision

- **One fresh engine instance per run, hosted in an iframe.** The gallery
  creates a new iframe for each sample/Run and destroys the previous one.
  This gives state isolation for free and releases the run's WebGL context on
  navigation (browsers cap live contexts and expose no clean JS destroy).
- **The host supplies the entry source before boot via a single global
  channel.** `src/web/entry.js` prefers `globalThis.__efx_main_js` over the
  resource-root `main.js`, consumes and deletes it before evaluation, and
  falls back to the existing resource-root behavior when it is absent. The
  channel is never exposed to the script, so the no-browser/host-dependency
  contract (ADR 0008) is unchanged. The host may also supply an asset-root
  URL (`globalThis.__efx_assets`, or `?assets=`); `entry.js` fetches and
  mounts that single zip before evaluating the entry source (F6a, ADR 0031),
  so the run's `load*` calls resolve against host-provided assets.
- **The gallery is a separate host application.** `gallery/` is a Vite +
  TypeScript + Svelte site whose samples are the committed golden scenes
  (catalog generated at build time) plus a curated showcase set. It embeds
  the unmodified `player_web.*` output; `docs/js-api.md` and
  `gallery/src/api/efx.d.ts` are the API contract and are updated with it.
- **Runs are explicit, not keystroke-live.** A Run/Reset control drives a
  fresh run; the embedding contract does not include a live eval/reset API.

## Consequences

- Future web work must treat "boot once, per-document engine, host-provided
  source" as the embedding contract; adding in-place script swapping means
  designing resource/hook/display-list teardown and revisiting this ADR.
- The engine hook is host-only: it adds no script-visible API, no `js-api`
  spec delta, and no new resource type, so game scripts and the desktop
  player are unaffected (absent override = unchanged behavior).
- `player_web.*` remains the single web build; the gallery reuses it rather
  than introducing a `-sMODULARIZE` target, keeping the downloadable web
  bundle and the verification spec's output set intact.
- `efx.d.ts` becomes a maintained API document that grows in lockstep with
  `docs/js-api.md`; the Pages workflow now builds the gallery site.

## Rejected alternatives

- **`-sMODULARIZE` + one long-lived page instance, re-instantiated per run.**
  Lost: re-instantiating the module in one document leaks WebGL contexts
  (no clean release) and still needs runtime-method exports for FS access.
- **A live eval/reset bridge API in one instance** (`efx_bridge_eval_script`
  plus teardown of hooks, display list, and every GC-finalized GPU handle).
  Lost: instant edits are not worth a milestone-scale teardown problem with
  real risk against ADR 0011/0012 resource ownership.
- **Normal-mode `?root=` plus writing edited code into MEMFS.** Lost: needs
  `-sMODULARIZE`/exported runtime methods to reach FS before `main()`, and
  passing a string is simpler than writing a virtual resource root.
- **Post-boot injection.** Lost: the frame loop is already running; there is
  nothing to swap into without the reset API rejected above.
- **Bundling Monaco instead of loading it from a CDN.** Lost: multi-MB in the
  Pages artifact and cache churn for an editor that degrades gracefully to a
  plain text surface when the CDN is unavailable.
