# Design

## Context

Fog is one more member of the value-snapshot state family (`efx_camera3d`,
`efx_light_set`, clear color) in `render_state` (see proposal.md — Why). The
canned mesh shader (ADR 0015/0021) already receives `camera_pos` and the
interpolated fragment world position (`efx_world_pos`), so view distance is
computable in the fragment stage with no new varyings and no depth-buffer
read. Record snapshots already carry the light set per mesh record; fog rides
the same mechanism. The unlit path currently early-outs from lighting before
any fog code would run — that early-out is preserved verbatim, which makes
"unlit is fog-free" free.

## Goals / Non-Goals

**Goals:**

- One fog state, uniform-shaped for the canned shader: two new `vec4`s in
  `fs_params`.
- Bit-comparable CPU reference for the fog factor (unit-test parity, mirroring
  the `efx_lighting_shade` pattern).
- Full plumbing parity across desktop (quickjs) and web (bridge) bindings.
- Golden scenes pin the visual contract (linear ramp + exp2) on all four
  targets.

**Non-Goals:**

- Fog on the quad/billboard/particle path (their vertices carry pre-divided
  NDC z, not view distance; CPU-baked fog factors are a future change).
- Fog on unlit materials (spec'd: unlit stays fog-free — skybox convenience).
- Post-chain interaction: fog is in-scene shading, applied before F5b;
  the post chain consumes fogged color unchanged.

## Decisions

### D1: Fog lives in the lit path only; the unlit early-out stays (chosen per user)

The unlit branch returns before lighting and before fog. Rationale: skyboxes
are rendered unlit, and fogging the sky would wash out the horizon the fog is
supposed to blend into. The lit path applies `mix(col, fogColor, factor)`
after the final clamp, alpha untouched. Rejected alternative (fog applied to
both paths, including unlit) — visually "correct" for atmosphere but breaks
the skybox idiom; lost because unlit is this engine's skybox/hologram escape
hatch and fog would defeat it.

### D2: Two vec4 uniform packing, mode as a float selector

`fs_params` gains `fog_color` (rgb + `a` = enabled flag, mirroring
`point_color[i].w`'s enabled pattern) and `fog_params` (`x` = mode
0/1/2, `y` = density, `z` = near, `w` = far). The shader branches on mode
inside one `if` chain guarded by the enabled flag. Rejected: three separate
fog pipelines (permutation per mode) — violates the one-uniform-driven-shader
rule (ADR 0026 precedent); rejected: packing color into the params vec4 —
costs clarity for zero uniform savings (we are at two vec4s either way).

### D3: Snapshot in the mesh record, not a playback-global

`efx_mesh_record` gains an `efx_fog fog` member snapshotted at record time,
next to `lights`. This follows ADR 0019 (value-snapshot state) and enables
mid-frame fog changes and per-target fog (cutscene segments into render
targets). The record union grows by ~28 bytes; the record budget
(`EFX_RENDER_RECORD_BUDGET_BYTES`) self-adjusts since it is charged per
`sizeof(efx_record)`; quad/billboard/particle records are untouched.

### D4: Struct shape mirrors the light set

```c
typedef struct efx_fog {
    int enabled;
    int mode;         /* EFX_FOG_LINEAR / EFX_FOG_EXP / EFX_FOG_EXP2 */
    float color[4];   /* rgb used; alpha ignored (light-set convention) */
    float near_z, far_z;  /* linear mode */
    float density;        /* exp modes */
} efx_fog;
```

with `efx_render_set_fog(const efx_fog *)` / `efx_render_fog(efx_fog *out)`
beside the light setters. `mode`/`enabled` validation happens in the binding;
the render module clamps out-of-range factors at evaluation, not at set time
(value state accepts what the shader accepts; the wire native is the
validation boundary, matching the light bank).

### D5: CPU reference extends the shade contract

The lighting CPU reference (`efx_lighting_shade`) gains fog application *for
lit results only* — either a new parameter or a parallel
`efx_fog_apply(fog, view_distance, color)` helper composed by callers of
`efx_lighting_shade`. Leaning: separate helper + explicit call sites, so the
lighting reference keeps its exact existing signature and existing tests are
untouched; fog parity tests target the helper. Final call (parameter vs
helper) is an implementation detail for the apply phase.

### D6: API surface and validation mirror `setLight`

Prelude: `g.setFog` with `__efxCheckKnown(opts, { color, mode, near, far,
density }, 'setFog')`, mode-string switch, required-field checks per mode,
RangeError for inverted/NaN ranges and non-positive density, then one
positional wire call `natives.setFog(enabled, mode, r, g, b, a, near, far,
density)`. `null` maps to `enabled = 0`. Web bridge adds
`efx_bridge_set_fog` mirroring `efx_bridge_set_point_light`; the web natives
fragment gains the same entry. `setFog` is registered in the graphics
namespace beside `setLight`.

### D7: Golden scenes

Two scenes, llvmpipe-captured per docs/verification-server.md: a linear ramp
(a ground plane receding from the camera, fog `near=2 far=20`) and an exp2
haze (same plane, `density=0.09`) — enough to pin both factor curves and the
fog color blending; the unlit exemption is covered by unit tests, not goldens
(a fogged skybox golden would be redundant with the lit golden).

## Risks / Trade-offs

- [Billboards/particles stay crisp in foggy scenes] → documented exclusion in
  the reference doc text (via the `.d.ts` comment) and `docs/js-api.md` gets
  no new rule (option-bag conventions already cover the shape); follow-up
  change may add CPU-baked billboard fog.
- [Shader branch on mode costs a little per-fragment] → the branch is
  uniform-coherent (same path for all fragments), negligible on all four
  backends; goldens catch any driver divergence.
- [Record size growth touches the documented quad capacity] → budget is
  derived from `sizeof(efx_record)`; add a unit assertion that the documented
  capacity still holds (~170k quads) if a test already pins it.
- [Web/desktop validation drift] → both paths call the same prelude
  validators on top of wire natives; type-test in the gallery pins the
  `.d.ts`.

## Migration Plan

Purely additive: no existing behavior changes (fog defaults off, existing
shader output byte-identical when fog is disabled — the goldens must show zero
diff on existing scenes). Rollback is reverting the change; no persisted
state, no resource lifetime implications.

## Open Questions

None — the model set, scope, unlit exemption, and spec placement were settled
during exploration.
