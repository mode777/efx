# Design

## Context

See `proposal.md` — Why. The player today accepts its root only at launch
(`src/player/player.c`), and the engine is a one-game-per-process lifecycle:
`efx_runtime_new` installs the API/prelude, the entry runs once after the
surface exists (`on_init`), and teardown happens only at exit. The platform
event callback (`src/platform/platform.c:175`) already receives all backend
events but does not enable drag-and-drop. On web the run is boot-driven
(`src/web/js/boot.js`): a host channel (`__efx_assets` / `?assets=`) is fetched,
written into MEMFS, and mounted before `main.js` is evaluated.

This change deliberately does **not** introduce a game-swap lifecycle; it
restarts the run. `in-place-game-swap` (follow-on) replaces the restart
mechanism while the observable drop behavior specified here stays.

## Goals / Non-Goals

**Goals:**

- One host-level drop path that works on all four targets, with desktop
  accepting a zip or a folder and web accepting a zip.
- Reuse the existing resource-root semantics unchanged; no script-facing
  surface, no async `load*`.
- Keep the change small and independently shippable, buying UX now and
  isolating the risky lifecycle work into the follow-on change.

**Non-Goals:**

- Preserving the window/GPU context across a swap (follow-on change).
- A web directory mount or dropped-folder support on web.
- Fixing the pre-existing `?assets=` query-param observability (tracked as a
  separate concern, not this change).
- Any new archive format or resource type.

## Decisions

### D1 — Detect drops through sokol on desktop, JS on web

Enable `sapp_desc.enable_dragndrop` and handle `SAPP_EVENTTYPE_FILES_DROPPED`
in `efx_event_cb` for native targets, using `sapp_get_num_dropped_files()` /
`sapp_get_dropped_file_path()`. On web, install `dragover`/`drop` listeners on
the canvas in the boot JS and read the `DataTransfer` `File`.

*Why the split:* persistence across a reload and reading a browser `File` are
inherently JS; marshalling a `File` object through C buys nothing. Native paths
come from sokol for free.

*Alternatives considered:* a single sokol path on web via
`sapp_html5_fetch_dropped_file` — rejected because the fetched bytes still have
to be persisted to JS storage for the reload, so the C round-trip adds
complexity without removing the JS work. Raw per-OS drop handling — rejected;
duplicates sokol.

### D2 — Desktop loads by relaunching the player

On a valid desktop drop, spawn a new player process bound to the dropped root
(zip or directory) and terminate the current run. Resolve the current
executable (`/proc/self/exe`, `_NSGetExecutablePath`, `GetModuleFileNameW`)
and spawn detached (POSIX `fork`+`execv`; Windows `CreateProcess`).

*Why:* it reuses the existing `player <root>` code path exactly and needs no
lifecycle surgery. Spawning detached rather than `execv`-ing in place avoids
threading a relaunch sentinel through the platform layer, which is important
because macOS's Cocoa loop never returns (ADR 0007) and exits the process
directly.

*Alternatives considered:* `execv` after teardown — rejected for the macOS
never-returns constraint and process-image replacement while the loop is live.
In-place swap — deliberately deferred to `in-place-game-swap`.

### D3 — Web loads by stashing the archive and reloading

On a valid web drop, write the dropped archive bytes into IndexedDB under a
one-shot token, put the token in the URL fragment, and `location.reload()`. On
boot, if the token is present, read the archive from IndexedDB, write it into
MEMFS, mount it via `efx_bridge_set_root`, delete the token and the stored
blob, then proceed with the existing entry evaluation.

*Why:* a reload is the web analogue of the desktop relaunch and needs no
in-place runtime reset. IndexedDB is required because a blob URL does not
survive a reload and `sessionStorage` is too small for archives.

*Alternatives considered:* blob URL + reload — rejected (object URLs die with
the document). In-place `efx_bridge_set_root` + re-run without reload —
rejected (the boot guards and runtime state make this the follow-on change's
problem).

### D4 — Validate a drop before committing to it

A drop is accepted only if it opens as a resource root containing `main.js`.
Desktop validates directly with `efx_resource_open` + `efx_resource_load_text`
before relaunching. Web validates by writing the bytes to a scratch MEMFS path
and calling a small bridge helper that opens the archive, checks for `main.js`,
and closes it — without touching the active root — and only then stashes and
reloads. An invalid drop prints/surfaces a diagnostic and leaves the current
run untouched (spec: "Unusable drop is rejected").

*Why:* keeps a bad drop from destroying a running game, and keeps the
diagnostic on the same channel as other root failures.

*Alternatives considered:* stash-and-reload-then-fail — rejected; it would
violate the "current run continues" scenario.

### D5 — A dropped item is a single root; first usable wins

A multi-item drop selects the first item that is a usable root; if none is
usable, the drop is rejected (D4). This is recorded as an assumption rather
than a user prompt.

### D6 — No new script-visible resources

This change adds no dynamic-count or fixed-bank script resources; nothing new
is exposed through GC-finalized classes or pre-allocated banks. The drop channel
is host-only and never reaches the script sandbox.

## Risks / Trade-offs

- **Window flicker / restart latency on desktop** → inherent to the relaunch
  strategy; accepted for this change and removed by `in-place-game-swap`.
- **Web reload loses all run state** → accepted; the dropped game is a fresh
  run by definition.
- **Zip bomb / oversized drop** → cap the accepted archive byte size before
  stashing or mounting (fixed limit, chosen in tasks); the provider still
  decompresses entries to memory.
- **Process spawn portability** (executable path, detach, quoting) → isolate in
  one helper per OS and cover with a native smoke test.
- **IndexedDB availability / private mode** → if storage fails, surface the
  drop failure through the standard channel instead of reloading.
- **macOS `_exit` path** → spawning detached before requesting quit keeps the
  current behavior intact; no platform-layer sentinel needed.
- **`?assets=` remains observable via `location.search`** → pre-existing,
  explicitly out of scope here (see Non-Goals); noted so it is not mistaken for
  a regression introduced by the drop channel.

## Migration Plan

Additive; no data or API migration. Rollback is removing the drop handlers and
the `enable_dragndrop` flag. The four-target gate (ADR 0020) still applies.

## Open Questions

- Exact archive-size cap value (settle in tasks against the largest curated
  pack).
- Whether the desktop spawn should reuse `argv[0]` or always resolve the
  executable path (resolve during apply per OS).
