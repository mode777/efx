# 0056 — A dropped resource root is a host-level feature loaded by restarting the run

Status: Accepted (2026-10, change `drop-to-load-game`)

Supports: vision.md (a game is a resource root); ADR 0007 (run modes and the
exit-code contract); ADR 0031 (the dir/zip provider); ADR 0030 (the host-only
web entry channel). A follow-on change (`in-place-game-swap`) is expected to
replace the desktop restart mechanism with an in-place swap and record its own
ADR.

## Context

The player accepted its resource root only on the command line, so trying a
packaged game meant relaunching the binary. The platform layer already receives
all backend events but did not enable drag-and-drop. Loading a new game at
runtime would require a lifecycle the engine does not have — the runtime, GPU
resources, input, physics, particles, and audio are built once and torn down
only at exit. The question was how far to go for a first version.

## Decision

- **Dropping a root is a host-level action.** The player detects a file/folder
  dropped on its window (native, via sokol `enable_dragndrop` +
  `SAPP_EVENTTYPE_FILES_DROPPED`) or on its canvas (web, via a boot-JS `drop`
  listener). It exposes **no** script-facing API, no loading hook, and no
  asynchronous `load*`; game scripts are unchanged and remain bound by the
  no-browser/host-dependency rule.
- **Accepted roots.** Native accepts a zip archive **or a directory**; web
  accepts a zip archive only (browsers do not expose a dropped directory as a
  filesystem root). A root is accepted only if it opens and contains `main.js`
  (`efx_resource_probe_root`); native additionally rejects archives above a
  fixed byte cap. An unusable drop prints a diagnostic and leaves the running
  game untouched.
- **Loading restarts the run.** On native the player spawns a detached copy of
  itself bound to the dropped root and exits 0. On web the dropped archive is
  validated, stashed in IndexedDB under a one-shot URL token, and the document
  reloads; boot consumes the token, mounts the archive through the existing
  provider, and evaluates its `main.js`. This reuses the command-line root path
  and needs no runtime-reset lifecycle.
- The observable behavior is pinned by `openspec/specs/player-runtime` and
  `openspec/specs/web-gallery`.

## Consequences

- Dropping a game works on all four targets with no new script surface.
- Native shows a brief window teardown/recreate and web reloads the document;
  both are accepted costs of the first version. ADR 0057 replaces the desktop
  mechanism with an in-place swap that keeps the window and GPU context.
- The web provider stays zip-only; a directory drop on web is out of scope.
- A dropped archive is untrusted input: it is size-capped and path-escapes are
  still rejected by the provider.

## Rejected alternatives

- **An in-place swap in this change:** deferred to ADR 0057 — it needs a
  session lifecycle across runtime, render, input, physics, particles, and
  audio, which is too much risk for the first version.
- **A script-visible drop callback or validation API:** reintroduces the
  loading hook and asynchrony the engine deliberately avoids (ADR 0031).
- **Handling native drops outside sokol (raw per-OS drag-and-drop):**
  duplicates what sokol already provides on all native targets.
- **A synchronous XHR/`execv`-in-place desktop relaunch:** the former is
  unavailable/worker-hostile; the latter fights macOS's never-returning Cocoa
  loop and process-image replacement mid-frame.
