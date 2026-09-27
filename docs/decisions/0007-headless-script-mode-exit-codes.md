# 0007 — Player run modes with an exit-code contract

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`; the third
console mode was added by F6d, change `2026-09-27-f6d-repl`)

## Context

The F1 verification gate — and every later milestone's automated checks
— must run on machines with no display, and assertions need a
machine-readable success signal that behaves identically across the four
targets (stdout/stderr flushing differs between them).

## Decision

The desktop player has three run modes:

- `player <resource-root>` — evaluate `main.js`, open the sokol window,
  run the frame loop (each frame: C dispatches JS `update`, then JS
  `render`); exit 0 on window close or `efx.quit(0)`. Hooks are looked
  up once after evaluation; missing ones are skipped.
- `player --script <file> [args…]` — **no sokol initialization at all**;
  evaluate the script and propagate its requested exit code.
- `player --repl [<root>]` — open the normal window and frame loop, then
  read JavaScript from stdin a line at a time and evaluate each line in
  the persistent global context (F6d). An optional root supplies the
  resource provider and runs its `main.js` once before input. `.help`
  and `.exit` are host commands handled by the player, not `efx` API.
  The console is desktop/embedded-runtime only.

Exit codes: `0` success; `1` generic runtime failure (missing
root/entry script, uncaught exception, file-not-found); otherwise the
script-requested code. `--repl` adds: EOF and `.exit` exit `0`,
`efx.quit(code)` exits `code`, a bad root is a diagnostic plus non-zero
exit, and a line that throws is recovered (printed, not fatal) so it
cannot corrupt the exit code.

Test scripts assert via exit codes only; log output is diagnostic,
never an assertion.

## Consequences

- Every milestone's automated gate is expressible as scripts + exit
  codes, identical on Win/Linux/macOS/Emscripten.
- Cross-target flushing differences cannot corrupt test results.
- Windowed frame-loop behavior is not covered by automated tests until
  F2's golden-image harness; until then it stays a manual per-platform
  checklist.
- The console reuses the resource-root window loop and the persistent
  context, so it exposes the existing namespace and adds no API.

## Rejected alternatives

- Windowed script mode in F1: rejected — no draw API exists yet to
  justify it.
- A blocking readline loop with no window for the console (F6d):
  rejected — it loses the live visual feedback that makes the console
  useful for drawing and posing.
- A separate stdin thread for the console (F6d): rejected — sokol and
  quickjs are main-thread-bound; polling stdin in the frame callback
  avoids cross-thread evaluation.
