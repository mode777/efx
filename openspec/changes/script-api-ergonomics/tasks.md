# Tasks

## 1. Math layer

- [x] 1.1 Add native `mat4.transform(pos, yawDeg?, pitchDeg?, rollDeg?, scale?)` (GLM composition `T·Ry·Rx·Rz·S`, fresh 16-array out) with prelude wire validation (finite angles/pos, positive scale, unknown-shaped arrays throw per convention); unit test vs hand-composed `translate/rotate/scale` chains passes
- [x] 1.2 Add pure-JS `efx.math.clamp(v, lo, hi)` and `efx.math.lerp(a, b, t)` in the shared prelude; unit test covers values, clamping, and argument non-mutation
- [x] 1.3 Regenerate `src/prelude/prelude.h` (`python3 tools/gen_prelude.py`) and verify the Linux gate's `--check` passes

## 2. Time

- [x] 2.1 Add the native frame clock (`efx.time.now`): frame-start snapshot, dt accumulator with 0.05s max-step clamp, monotonic; unit test drives frames through the runtime seam and asserts snapshot equality within a frame, strict increase across frames, and the clamp after a long stall
- [x] 2.2 Register the `time` sub-namespace in the desktop binding and the web bridge; headless smoke test reads `now` in update and render hooks

## 3. Graphics: screenRay + primitives

- [x] 3.1 Implement `efx.graphics.screenRay(x, y)` over the live 3D camera (frame-space input per task 4.1's mapping, eye origin, normalized direction, plain-data return, `Error` before any `setCamera3D`); unit test: ray through the frame center of a camera targeting point P passes through P; cross-runtime check
- [x] 3.2 Add `uvs` (number or `[u, v]`, positive, default 1) to `makePlane` in the prelude primitive generator; unit test asserts uv spans for `1`, `6`, and `[6, 3]`, and `RangeError` for non-positive values

## 4. Input: frame-space pointer

- [x] 4.1 Add the surface→frame mapping in `api_input.c` (scale-only, most recent `setCamera2D` frame, identity fallback when frame unset; applies to `position`/`x`/`y`/`delta` and mouse event payloads; `wheel` unscaled); injection-seam unit test: with frame `[640, 360]` on a 1280×720 surface, a pointer event at surface `(640, 360)` reports `(320, 180)`
- [x] 4.2 Unit test the fallback: with no `setCamera2D` ever called, injected pointer events report surface pixels unchanged

## 5. Audio: engine-managed unlock

- [x] 5.1 Fire `efx_audio_request_resume()` once from `efx_input_key_down`/`efx_input_mouse_down`; unit test: with a stub resume callback, a synthetic key-down after audio use triggers exactly one unlock call
- [x] 5.2 Remove `efx.audio.resume` from the desktop binding (`api_audio.c`) and the web bridge (`src/web/js/audio.js`); verify playback-handle `pause()`/`resume()` are untouched; error-catalog run shows no audio-namespace resume entries on both runtimes

## 6. Physics: teleport + debug draw

- [x] 6.1 Implement `Body.setPosition(p)` and `Character.setPosition(p)` (instant placement, zeroed linear/angular velocity, wake, validation per convention, destroyed-body throw); unit test: resting/asleep dynamic body teleports, reports zero velocity, and falls under gravity; character case covered; cross-runtime error check
- [x] 6.2 Implement `efx.physics.debugDraw(opts?)` (unlit translucent hulls per live body/character into the display list, `{ color, alpha }` options, render-phase-only enforcement); unit test: records appear for all live bodies, destroyed bodies are skipped, update-phase call throws, bad options throw per convention

## 7. 2D tolerance + color acceptance

- [x] 7.1 Change `drawQuad` size validation (`api_2d.c:266` path): finite entries ≤ 0 record nothing without throwing; non-numeric/infinite entries keep throwing; unit test covers both paths and render-state non-pollution
- [x] 7.2 Apply the same degenerate-size rule to `drawBillboard`; unit test mirrors 7.1
- [x] 7.3 Add 3-component color acceptance in the shared color parse (prelude wire + C parser): `[r, g, b]` = opaque; wrong-length/type/non-finite keep current errors; unit test sweeps a draw tint, material channel, light color, and clear color on both runtimes

## 8. Script API surface + docs

- [x] 8.1 Update `gallery/src/api/efx.d.ts` (screenRay, transform, clamp/lerp, time namespace, setPosition on Body/Character, debugDraw, makePlane `uvs`, Color union, mouse frame-pixel docs, audio resume removal) and pass `npm --prefix gallery run check`
- [x] 8.2 Regenerate `docs/api/` (`npm --prefix gallery run docs:markdown`) and pass `npm --prefix gallery run docs:check`
- [x] 8.3 Update `docs/js-api.md` where the layering rule (math no longer purely `[JS]`), input units (frame pixels), and audio unlock ownership are stated
- [x] 8.4 Write ADR `docs/decisions/0060` (frame-space input units, native hot math composition, engine-managed audio unlock, degenerate-draw tolerance) and index it in `docs/decisions/README.md`

## 9. Corpus migration + verification

- [x] 9.1 Migrate curated samples: delete `toFrame`/`readWindow` mapping (audio-showcase, input-playground, game-bot-arena, game-mini-golf, game-bloom-breakout, game-neon-pong), audio unlock gates (audio-showcase, game-bot-arena, game-bloom-breakout, game-glow-gauntlet), recreate-on-teleport workarounds (game-bot-arena, game-mini-golf, game-sky-steps), hand-rolled `clamp`/clock/matrix ladders where the new helpers apply; each edited sample runs headless (`--script` smoke or gallery build) without error
- [x] 9.2 Add a golden scene exercising a zero-size quad between valid draws (frame must equal the scene without the degenerate call) and capture llvmpipe goldens per `docs/verification-server.md`; `ctest` golden suite passes on an ON build
- [x] 9.3 Extend the cross-runtime error catalog for the new validation messages (`transform`, `setPosition`, `debugDraw`, `screenRay`, color lengths) and verify byte-identical outputs
- [x] 9.4 Update portable script tests for mouse units and removed `resume`; full unit suite green locally (`cmake -B build -DEFX_HEADLESS=ON && ctest -E golden`)
- [x] 9.5 Verify on the SSH server first (`python3 tools/verify_remote.py all script-api-ergonomics`), then dispatch the gate (`gh workflow run ci.yml --ref script-api-ergonomics`) and iterate Linux → Windows → macOS to green
- [x] 9.6 `npx openspec validate "script-api-ergonomics" --type change --strict` passes before archive
