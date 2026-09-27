# 0031 — Resource loading: a dir/zip provider and fetch-once-at-boot

Status: Accepted (2026-09, change `f6a-resource-loading`)

Supports: vision.md (resources from a folder or zip; a `res://`-style root);
ADR 0003 (module walls); ADR 0011/0012 (resource classes and memory
discipline); ADR 0016 (loading `main.js` is the implicit init); ADR 0022
(quickjs desktop-only; the browser engine drives the core); ADR 0030 (the
host entry-source channel).

## Context

F6 must load authored assets from a resource root that can be a directory or
a zip archive, on all four targets, while keeping the script-facing `load*`
API synchronous (as `docs/js-api.md` already promised). Desktop reads are
synchronous; the browser has no synchronous file fetch, but Emscripten's
`stdio` maps onto MEMFS, so bytes that are already mounted can be read
synchronously. The question is how remote assets get mounted without leaking
asynchrony into game scripts.

## Decision

- **One pure-C provider, `src/resource/`.** `efx_resource_open` accepts a
  directory or a `.zip`; `read`/`size`/`exists` address entries by
  root-relative path. It uses `stdio` only (desktop filesystems and MEMFS
  alike) plus vendored miniz (`vendor/miniz/`) for zip entries. No quickjs,
  no sokol (ADR 0003). `main.js` itself is read through the provider.
- **The player owns the root and threads it to the runtime/bridge.** Desktop
  `player <dir|zip>` and `--script <file> [--root <dir|zip>]` (default root:
  the script's directory); on web the bridge holds the provider and
  `entry.js` points it at the mounted root.
- **Web boot is fetch-once-then-synchronous.** When the host supplies an
  asset-root URL (`globalThis.__efx_assets`, or `?assets=`), `entry.js`
  fetches the single zip, writes it into the filesystem, points the provider
  at it, and only then evaluates the entry script and starts the frame loop.
  The channel is consumed before evaluation and is invisible to the script.
  With no URL the existing preloaded/MEMFS path is unchanged (Node harness,
  web goldens).
- **The script API stays synchronous everywhere.** There is no loading hook
  and no async `load*`: `loadText`/`loadImage`/`loadTexture` are plain
  synchronous calls once boot completes.

## Consequences

- Game scripts are identical on desktop and web: `load*` never returns a
  promise, and `main.js` can load assets at top level.
- The web embedding contract (ADR 0030) gains an optional host asset root;
  the gallery may pass a zip URL per run.
- Adding a remote-asset loading screen means changing the embedding page, not
  the engine; the engine exposes no loading hook by design.
- Future asset kinds (F6b glTF) reuse the same provider and read callback
  (relative URI resolution inside a zip), so no second file layer appears.
- A zip is buffered once in memory on web (fetched bytes + filesystem copy);
  acceptable at PS2-era asset sizes.

## Rejected alternatives

- **An async `load*` API**: leaks Promises into game scripts and diverges
  from the desktop contract; the whole point of mounting first is to avoid it.
- **Synchronous XHR**: deprecated, blocks the main thread, and is unavailable
  in workers.
- **Per-platform file providers**: duplicate logic and path rules for no
  benefit once `stdio` maps to MEMFS.
- **Per-file remote fetch / manifest**: more round-trips and a second
  resolution scheme; the zip is the packaging model vision.md already names.
- **A script-visible loading hook**: reintroduces asynchrony the host page
  already controls.
