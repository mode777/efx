# Proposal

## Why

`drop-to-load-game` loads a dropped root on desktop by relaunching the player:
the window is torn down and reopened, causing a visible flash and losing window
state. A runtime should swap games in place — keep the platform window and
Sokol GPU context alive while the previous game's script session and resources
are released and the new game starts.

## What Changes

- Introduce a stable **player session** object that owns the active runtime,
  resource provider, and lifecycle hooks, so per-frame callbacks no longer
  point directly at the runtime.
- Add a **desktop in-place swap** path: release the previous game's script
  session and engine state (runtime, GPU resources, input, physics, particles,
  audio), open the new root, and re-evaluate its entry — **without** recreating
  the platform window or the Sokol GPU context.
- Desktop dropped-root loading uses the swap instead of relaunching; it
  supersedes the desktop restart mechanism of `drop-to-load-game`. The
  observable drop behavior that change specified is unchanged (a dropped
  zip/folder loads that game).
- Web behavior is unchanged: the gallery continues to isolate each run in a
  fresh iframe (its existing contract) and the standalone web player continues
  to load a dropped root via `drop-to-load-game`'s reload. In-place swap is a
  desktop problem — there is no persistent C window to preserve on web.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `player-runtime`: add a requirement that the active game can be swapped in
  place on desktop — the platform window and rendering context survive, the
  previous game's state is fully released, and the new game's entry runs — with
  no residue and no accumulation across repeated swaps.

## Impact

- `src/player/player.c` — session struct owning `efx_runtime` + provider +
  hooks; a frame-start swap step; `hooks.ud` points at the session, not the
  runtime.
- `src/platform/platform.c` — a dropped root requests a swap instead of a
  relaunch/quit; the window and `sg` context stay up.
- `src/runtime/` — a reset path (destroy + recreate) that runs JS finalizers
  before GPU teardown; hook lists and physics world rebuilt.
- `src/render/` — a resource-teardown reset that preserves the installed sink,
  plus re-deriving the engine-owned white texture view and re-applying default
  state; verify the `sg` context and pipelines survive and are reusable.
- `src/input/`, `src/physics/`, `src/render/render_particles.c`,
  `src/platform/audio_backend.c` — clear/reset per-game state.
- Tests: a native swap smoke (repeat swaps release GPU resources and start
  clean) plus the existing drop smoke; four-target gate (ADR 0020) applies.
- Docs: `docs/decisions/` — **new ADR docs/decisions/0057** recording the
  desktop in-place session-swap lifecycle; it supersedes the desktop restart
  mechanism noted in ADR 0056 (mark the supersession in
  `docs/decisions/README.md`). No `js-api`/`efx.d.ts`/`docs/api` change (no
  script-facing surface).
- `AGENTS.md` — update the run-modes/player pointer to describe the session
  swap.

## Non-goals

- In-place swap on web / reusing one iframe across gallery samples; the gallery
  keeps its fresh-iframe isolation and the web player keeps `drop-to-load-game`'s
  reload.
- Any script-facing swap/reload API, loading hook, or async `load*`.
- Preserving game state, resources, or hooks across a swap (a swap is a fresh
  game).
- Hot-reloading edited source into a running game without re-evaluation.
- Concurrent games, multiple sessions, or background loading.
- Changing the drop-to-load observable behavior specified by
  `drop-to-load-game` (only its desktop restart mechanism is replaced).
- New archive formats or resource types.
