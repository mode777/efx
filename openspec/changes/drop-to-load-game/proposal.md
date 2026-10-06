# Proposal

## Why

The player only accepts a resource root on the command line, so trying a game
means restarting the binary (or, in the gallery, selecting a catalog entry).
Users expect to drop a packaged game — a zip, or a folder on desktop — onto the
running window/canvas and have it load, which makes the player feel like a
runtime rather than a launcher.

## What Changes

- The player detects a file/folder dropped onto its window (desktop) or canvas
  (web) and loads the dropped path as the new resource root.
- **Desktop** accepts a dropped zip archive **or a dropped folder**; **web**
  accepts a dropped zip archive (browsers do not expose a dropped folder as a
  filesystem root).
- The gallery runner lets a dropped archive replace the running sample,
  preventing the browser's default open-the-file behavior.
- Dropping is a **host-level** action: it exposes no script-facing API, no
  loading hook, and no asynchronous `load*`. Game scripts are unchanged and
  remain bound by the no-browser/host-dependency rule.
- This change loads the dropped game by **restarting the run** (relaunch the
  process on desktop; reload the page/iframe on web). Keeping the window and
  GPU context alive across a swap is the follow-on `in-place-game-swap` change.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `player-runtime`: add a requirement that a resource root dropped onto the
  player's window (desktop) or canvas (web) is loaded and run as the game,
  covering desktop zip and folder and web zip, and the failure behavior for a
  drop that is not a usable root.
- `web-gallery`: add a requirement that the runner accepts a dropped archive
  as a sample run and suppresses the browser's default file-open handling,
  without breaking the gallery shell.

## Impact

- `src/platform/platform.c` — enable sokol drag-and-drop and route
  `SAPP_EVENTTYPE_FILES_DROPPED` into the player; desktop dropped paths and web
  dropped file bytes.
- `src/player/player.c` — desktop relaunch with the dropped root (zip or
  folder) and clean exit of the current run.
- `src/web/js/boot.js` + `src/web/bridge_resource.c` — web drop writes the
  dropped archive into the filesystem and reloads the run bound to it.
- `gallery/public/runner.html` — accept a drop on the embed and forward it.
- Tests: player smoke coverage for the drop path (native, display-capable
  build) and a web/gallery drop test; the existing four-target gate (ADR 0020)
  still applies.
- Docs: `docs/decisions/` — **new ADR docs/decisions/0056** recording that a
  dropped root is a host-level feature (no script API; desktop zip/folder, web
  zip) loaded by restarting the run. No `js-api`/`efx.d.ts`/`docs/api` change
  (no script-facing surface). `AGENTS.md` run-mode pointer may need a line.
- No ADR supersession in this change; the follow-on `in-place-game-swap`
  replaces the restart mechanism and records its own ADR.

## Non-goals

- Keeping the window/GPU context alive across a swap (follow-on change).
- A script-visible drop callback, drop validation API, or any async `load*`.
- Dropping folders on web, or a web directory mount.
- Multi-file drops beyond a single root; dropping a zip/folder plus unrelated
  files is treated as a failure (or the single root is chosen — settled in
  design).
- Archive formats beyond what the resource provider already reads.
- Windows/macOS process-relaunch ergonomics beyond launching the new run and
  exiting the old one.
