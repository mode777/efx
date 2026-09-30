# 0046 — The physics world holds live `Body`/`Character` wrappers; GC never removes a collider

Status: Accepted (2026-09, direct fix — no openspec change; specs
`collision-physics`, `character-controller`, `js-api` edited in place)

Amends: ADR 0011/0012 (the GC-finalizer backstop) for the F12 classes only;
ADR 0040 (F12 physics core). Corrects the diagnosis in ADR 0045's context.

## Context

Since F10 the desktop `main.js` runs as a CommonJS module function, so a
top-level `const ground = efx.physics.createBody(...)` that no hook closure
captures is freed by QuickJS reference counting as soon as the module
returns. The `Body` finalizer destroyed the native collider, so the gallery
physics showcase lost its ground, walls, ramp, and sensor before frame 1 and
every prop, including the swept `moveAndSlide` character, fell through the
floor on Windows/Linux/macOS. The web bridge has no finalizer, so the
browser was unaffected. ADR 0045 blamed this on large-`dt` tunneling;
sub-stepping is still valid for thin geometry, but it did not cause this
failure. Headless `--script` tests missed it because they step inside the
module body while every local is still alive.

## Decision

A live `Body` or `Character` is world state, not a script-owned resource.
In `src/api/api.c` each wrapper keeps an owned reference to itself
(`pinned`) from creation until `destroy()`, `efx.physics.clear()` (which
also marks every wrapper destroyed), or runtime teardown
(`efx_api_physics_release`, called by `efx_runtime_destroy` before the
context is freed). Only after unpinning can the finalizer run, and by then
the native collider is already gone. Both runtimes now behave the same way:
dropping every script reference never changes the simulation.

## Consequences

- Scripts must `destroy()` or `clear()` physics objects they no longer
  want; an unreferenced body keeps simulating (and costs memory) until then.
  This is the same behavior the web binding always had.
- Any future native class whose object takes part in engine-owned state (as
  opposed to being only a handle to data) needs the same pinning. The ADR
  0011 finalizer backstop applies only to resources nothing but the script
  can observe.
- `physics_smoke.js` covers colliders created in a scope that is already
  gone; physics regressions need a case like that, not just keep-alive
  locals.

## Rejected alternatives

- **Keep references in the sample.** Fixes one demo and leaves the trap for
  every script, and the desktop and web runtimes still disagree.
- **Add a `FinalizationRegistry` to the web binding to match desktop.** Makes
  the browser just as broken; GC timing would decide what the simulation
  contains.
- **Keep the native collider alive but let the wrapper die.** Contacts and
  raycast hits resolve bodies back to their wrapper objects; without a live
  wrapper they would report `null` for a body that still exists.
