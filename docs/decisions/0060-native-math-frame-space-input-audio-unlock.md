# 0060 — Hot-path math is native-backed, input speaks frame pixels, and audio unlock is engine-managed

Status: Accepted (2026-10, change `script-api-ergonomics`)

## Context

A duplication survey of the curated demos found the same plumbing
re-implemented per sample — pointer→frame mapping, camera picking rays,
model-matrix ladders, per-sample clocks and `clamp()`, audio unlock
flag/retry dances, and collider teleport workarounds. Three of those are
convention-level: where a hot math helper is implemented (ADR 0010/0049
layer rules), what coordinate space input reports (the input spec pinned
surface pixels), and who owns the browser autoplay unlock (the audio spec
kept an explicit script call). Evaluation record:
`openspec/changes/script-api-ergonomics/design.md`.

## Decision

**Native-backed hot math.** `efx.math.mat4.transform(pos, yaw?, pitch?,
roll?, scale?)` composes `T·Ry·Rx·Rz·S` in one marshal-only native call
(shared `src/math/transform.c`, called by both bindings) and returns the
same plain flat 16-number array as its `[JS]` siblings. Cold-path option
validation stays in the shared prelude (ADR 0049); the scalar helpers
`efx.math.clamp`/`lerp` and every pre-existing `mat4`/`vec3`/`quat` member
stay pure ES6. The `[JS]` layer rule is therefore about script-visible
*data* (plain arrays, degrees — ADR 0010), not implementation locus: a
helper whose contract is plain data may be native-backed when it is on a
hot path.

**Input reports frame pixels.** `efx.mouse` position/x/y/delta and mouse
event payloads report the **active 2D frame's** coordinate space (scale-only
mapping `surface px × frame/surface`; identity when no frame was ever set),
the same space `drawQuad` uses. Wheel deltas are unscaled, and
`efx.window.size`/`dpiScale` stay surface pixels — the browser-shaped split
(input in the logical/authoring space, the framebuffer behind an explicit
ratio). The mapping is applied once, at the input module's expose boundary
(`src/input/input.c` + the binding dispatch); 2D camera pan/zoom/rotation is
deliberately **not** folded in (Godot's viewport-vs-Camera2D separation), so
event coordinates are stable under camera motion. `graphics.screenRay(x, y)`
consumes the same space, so `screenRay(...efx.mouse.position)` picks the
drawn pixel.

**Audio unlock is engine-managed.** The engine fires
`efx_audio_request_resume()` once from the first key-down/mouse-down, via a
gesture callback the platform registers on the input core
(`efx_input_set_gesture_callback`) so the input module stays audio-free
(ADR 0003). There is no script-facing unlock entry point. Browsers do not
count gamepad input as a user gesture; that limitation is documented, not
worked around.

**Degenerate draw tolerance.** A finite `drawQuad`/`drawBillboard` size
component ≤ 0 records nothing and throws nothing; non-finite and
wrong-arity values keep their existing error classes.

## Consequences

- Future hot math helpers may follow the native-wire pattern; the bar is
  "plain-data contract + measured hot path", and the prelude keeps
  validation so cross-runtime errors stay byte-identical.
- Input coordinates are frame pixels on every target. Frame-less scripts
  (no `setCamera2D` frame) see surface pixels exactly as before, so 3D-only
  scenes are unaffected; a zoomed 2D camera still needs manual world-space
  math (a future accessor can layer on without breaking this).
- Scripts no longer manage the autoplay unlock; the first real gesture
  unlocks audio, and a gamepad-only web session waits for one key/click.
- `drawQuad`/`drawBillboard` callers may pass zero sizes freely (health
  bars, meters); a size that used to throw now silently no-ops.

## Rejected alternatives

- **Keep `mat4.transform` pure JS.** Lost: it is the highest-frequency
  per-draw composition; native avoids three intermediate matrices and four
  array allocations per call, and the two-layer rule protects *data shape*,
  not implementation.
- **Report input in surface pixels with a converter helper.** Lost: keeps
  the per-sample mapping dance alive and hands scripts device pixels — the
  one space browsers never use for input.
- **Fold the 2D camera inverse into input coordinates.** Lost: semantics
  shift under camera motion, wrong for HUD input, and Godot precedent keeps
  the stretch transform and Camera2D transforms separate.
- **Keep `efx.audio.resume()` as a documented no-op.** Lost: a script call
  from an update hook is never inside a user gesture, so the verb could only
  fail; keeping it preserves a dead surface and the retry boilerplate.
- **Clamp degenerate sizes to a minimum instead of no-op.** Lost: hides
  authoring bugs behind a 1-pixel sliver.
