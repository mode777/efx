# Design

## Context

See `proposal.md` for motivation. Constraints:

- **Run modes (ADR 0007):** resource-root mode runs the sokol frame loop;
  `--script` mode evaluates one file and exits, no window. F6d adds a third.
- **Embedded runtime (ADR 0002/0022):** quickjs-ng is desktop-only; the web
  build has no interpreter and no stdin console.
- **Resource root (F6a):** the provider and `load*` functions already exist and
  are usable from any evaluated snippet.
- **Hook registration (ADR 0016):** `update`/`render` hooks are explicit and
  can be registered/unregistered at any time — REPL-friendly by design.

## Goals / Non-Goals

**Goals:**

- A console that evaluates lines in the persistent context while the scene's
  frame loop keeps running.
- Reuse the existing namespace and resource root; add no script API.

**Non-Goals:**

- Line editing, history, completion, or multi-line continuation.
- Any web REPL; any engine-side introspection API.

## Decisions

### D1 — Windowed frame loop with stdin polling

The REPL reuses resource-root mode's window and frame loop. Each frame, the
player polls stdin non-blocking (POSIX `poll`/`read` on `STDIN_FILENO`; Win32
console/`_read` with a ready check), buffers a partial line, and evaluates
complete lines. A prompt is printed only when stdin is a TTY, so piped test
input does not pollute captured stdout.

- **Rejected — a blocking readline loop with no window:** loses the live visual
  feedback that makes the console useful for drawing/posing.
- **Rejected — a separate thread:** sokol and quickjs are main-thread-bound;
  polling in the frame callback avoids cross-thread evaluation.

### D2 — Persistent global evaluation, errors recovered

Each line is evaluated with a global-context eval in the runtime's existing
`JSContext`; because it is the same context, bindings and `let`/`const` live in
the global scope persist. When the completion value is not `undefined`, it is
printed; a thrown exception is formatted to stderr, its `JSValue` freed, and
the loop continues. This needs a small runtime helper that evaluates and
reports without setting the runtime's fatal error flag (the `--script` path's
error handling must not be reused, since it terminates).

- **Rejected — evaluating each line as a fresh script/context:** loses state,
  defeating the purpose.
- **Rejected — treating a throw as fatal:** makes the REPL unusable for
  iterative work.

### D3 — Host commands before evaluation

Lines beginning with `.` are host commands: `.help` prints usage, `.exit`
requests a clean quit. They are handled by the player, not by `efx`, so the
public API surface is unchanged.

### D4 — Optional root and entry script

`--repl [<root>]` reuses the F6a root. If the root has `main.js`, the player
evaluates it once before accepting input (same semantics as resource-root
mode minus the separate entry bootstrap); otherwise it starts bare. `load*`
resolves against the root.

- **Rejected — always requiring a root:** bare-namespace tinkering is a core
  REPL use case.
- **Rejected — never running `main.js`:** authors would hand-recreate scene
  setup to inspect it.

### D5 — Desktop-only, standard exit contract

On Emscripten the mode reports unavailability. EOF on stdin and `.exit` end
with code 0; `efx.quit(code)` ends with that code; a bad `--repl` root is a
diagnostic and non-zero exit.

## Risks / Trade-offs

- **[Display required]** → the windowed REPL test is display-gated like the
  golden suite (Linux under `xvfb-run`; Windows/macOS in the gate); it is
  excluded from the headless local configuration.
- **[Windows console polling differs]** → isolate the platform read behind one
  small function and keep the piped-stdin path (used by CI) uniform.
- **[stdout pollution from prompts/results]** → prompts only on a TTY; results
  printed only for non-undefined completion values, so scripted assertions can
  match output.
- **[Eval helper must not trip the fatal-error flag]** → add a dedicated
  evaluate-and-report path; unit-test that a throwing line leaves the runtime
  healthy and the next line runs.
- **[`let` redeclaration across lines]** → a second `let x` in the same global
  scope throws; acceptable for v1 and documented by example (use assignment).

## Migration Plan

1. Add the runtime evaluate-and-report helper and unit-test error recovery.
2. Add `--repl` parsing and the windowed input loop; test with piped stdin
   under a display.
3. Add host commands, the optional root/entry-script path, and the exit
   contract; test `.help`/`.exit`/`efx.quit`/EOF.
4. Update docs (`docs/js-api.md` note, `efx.d.ts` comment, AGENTS rows) and run
   the Linux → Windows → macOS gate order.

Rollback: additive run mode; reverting leaves the existing modes untouched.

## Open Questions

None that affect the specs, approach, or tasks. Multi-line continuation and
line editing are recorded non-goals for a possible future additive change.
