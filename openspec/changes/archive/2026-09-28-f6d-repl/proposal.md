# Proposal

## Why

F6's last deliverable is the interactive console the vision describes: a mode
where you can drive the `efx` namespace by hand. It is a testing and authoring
tool — inspect resources, try API calls, pose a mesh at an explicit time and
see the frame update — and it exercises the existing namespace rather than
adding a second surface.

## What Changes

- **`--repl [<root>]` run mode.** The desktop player opens its normal window
  and frame loop, then reads JavaScript from stdin a line at a time between
  frames, evaluating each line as a global script snippet in the same persistent
  quickjs context the entry script would use. State persists across lines.
- **Optional resource root.** When a root is given, its `main.js` (if present)
  runs first to set up the scene, and `load*` calls resolve against it; with no
  root the REPL starts with the bare namespace. `load*` therefore also works
  interactively (including with `--root`).
- **Error recovery.** A throwing line prints the error and the REPL continues;
  it does not exit the run. `efx.quit(code)` ends the run with that code; EOF on
  stdin shuts down cleanly with code 0.
- **Host commands, not API.** `.help` and `.exit` are REPL host commands
  handled before evaluation. No new `efx` function or property is added.
- **Desktop-only.** The REPL is part of the embedded-runtime (quickjs) player;
  on Emscripten it is not available (no stdin console).
- **Docs/contract.** `docs/js-api.md` and `gallery/src/api/efx.d.ts` note the
  REPL drives the same namespace (no new API); the player reference documents
  the run mode.

## Capabilities

### New Capabilities

- `repl`: the interactive console run mode — line evaluation in the persistent
  script context, optional resource root, host commands, error recovery, and
  the exit-code contract. Desktop/quickjs only.

### Modified Capabilities

- `player-runtime`: a third run mode (`--repl [<root>]`) in addition to
  resource-root and `--script` modes.

## Impact

- **New code:** REPL input loop and snippet evaluation in `src/player/` and
  `src/runtime/` (`efx_runtime_eval_repl_line` or equivalent, persistent
  context).
- **Modified code:** `src/player/player.c` (argument parsing, run mode,
  `on_frame` stdin polling), possibly `src/runtime/runtime.c` (evaluation
  helper that reports results without aborting the run).
- **Docs:** `docs/js-api.md` (REPL note), `gallery/src/api/efx.d.ts` (comment),
  `AGENTS.md` current-state/roadmap rows.
- **Milestone:** F6 (F6d slice). Predecessor slices must pass their gate first.
- **Verification:** a piped-stdin ctest case exercising evaluation, error
  recovery, `.help`/`.exit`, and `efx.quit` exit codes; desktop only.

**Non-goals (out of scope for F6d):**

- Any new script-facing API, resource type, or introspection function.
- Multi-line/continuation input, history, completion, or readline editing
  (candidate for a later additive change).
- A web/browser REPL (no stdin console; the gallery editor is the web analogue).
- Changing the `--script` or resource-root run modes.
