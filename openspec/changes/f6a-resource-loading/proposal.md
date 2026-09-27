# Proposal

## Why

F5b closed the render-target + post-FX milestone, so F6 (resource packaging +
glTF import + REPL) is next. The engine is still asset-blind: every golden and
sample script builds its pixels and meshes procedurally, and the player never
tells the API where the resource root is. F6a delivers the foundation the rest
of F6 stands on — a resource root that can be a directory **or a zip**, a
synchronous `load*` layer for text and decoded images, and the web boot path
that mounts a remotely fetched zip before `main.js` runs. It deliberately stops
short of glTF so the platform plumbing (provider, miniz, both bindings, async
boot) is proven before cgltf and the import mapping are layered on.

## What Changes

- **Resource root provider (new `src/resource/`).** A pure-C module that opens
  a root that is either a directory or a `.zip` and reads entries by relative
  `res://`-style path. `stdio` serves both desktop filesystems and Emscripten
  MEMFS, so one provider covers all four targets; zip access is miniz.
- **Player root handling.** `player <root>` accepts a directory **or** a zip.
  Desktop `--script <file>` gains an implicit resource root equal to the
  script's directory (no `--root` flag), so ctest asset fixtures need no extra
  plumbing. The entry `main.js` itself is read through the provider.
- **Image decoding.** `loadImage(path)` decodes PNG/JPEG to `ImageData`
  (`rgba8`, via the already-vendored `stb_image`), and `loadTexture(path)` is a
  pure-JS `createTexture(loadImage(path))` convenience. `loadText(path)` reads
  UTF-8 text.
- **Web: fetch-at-boot, synchronous API.** When the embedding page supplies an
  asset-root URL (`globalThis.__efx_assets`, or `?assets=`), the browser boot
  fetches the single zip, writes it into MEMFS, points the provider at it, and
  only then evaluates `main.js`. The script-facing API stays **synchronous on
  every target**; the async wait happens once at boot, not per `load*`. With
  no asset URL the existing directory/MEMFS path is unchanged (Node harness,
  web goldens, preloaded roots).
- **Dependency additions.** Promote `stb_image` (already vendored, F2) into the
  core and vendor **miniz** (MIT, single-file C) for zip reads. `cgltf` is
  deferred to F6b.
- **Docs/contract.** `docs/js-api.md` and `gallery/src/api/efx.d.ts` gain the
  F6a entries; a new ADR records the provider + async-boot decision, and
  ADR 0016/0030 are amended for the pre-boot mount and host asset channel.

## Capabilities

### New Capabilities

- `resource-loading`: the resource root contract (directory or zip, relative
  paths), the file provider, text/image decoding to engine resources, and the
  web boot behavior that mounts a host-provided zip before the entry script
  runs.

### Modified Capabilities

- `js-api`: catalog the new F6a script-facing functions (`loadText`,
  `loadImage`, `loadTexture`) and their error/lifecycle semantics.
- `player-runtime`: the resource root may be a zip as well as a directory;
  `--script` mode resolves `load*` against the script's directory; the web
  runtime may mount a host-provided asset root before evaluating `main.js`.

## Impact

- **New code:** `src/resource/` (provider, image decode); loader bindings in
  `src/api/` (quickjs) and `src/web/bridge.c` (native bridge).
- **Modified code:** `src/player/player.c` (root = dir|zip, `--script` root),
  `src/web/entry.js` (async boot, fetch/mount, read `main.js` through the
  provider), `src/runtime/` (root state), `CMakeLists.txt` (vendor miniz, stb
  into core).
- **Vendored:** `vendor/miniz/` (new); `vendor/stb/` promoted into `efx_core`.
- **Docs:** `docs/js-api.md`, `gallery/src/api/efx.d.ts`, new ADR
  `docs/decisions/0031-*` + index; amendments to ADR 0016 and ADR 0030.
- **Milestone:** F6 (F6a slice). Predecessor gate (F5, archived) is green.
- **Verification:** smoke/script tests reading from a directory and a zip
  fixture; a golden that samples a decoded PNG; a web harness case that fetches
  a zip from the local test server before boot.

**Non-goals (out of scope for F6a):**

- glTF/GLB parsing and mesh/material import (F6b).
- Skin and animation clip import (F6c) and REPL mode (F6d).
- Runtime fetching of individual files (manifest) or of remote cross-origin
  assets; F6a fetches exactly one zip at boot.
- Color management / sRGB handling and texture sampler wrap/filter control.
- Any script-visible asynchronous API — `load*` stays synchronous.
