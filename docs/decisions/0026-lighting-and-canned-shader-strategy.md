# 0026 — Lighting is world-space Phong from one uniform-driven mesh shader; no F4a permutations

Status: Accepted (2026-09, change `f4a-lighting-phong`)

Supports: vision.md — "Simple phong material system: Ambient, Diffuse,
Specular, Emissive"; roadmap F4 ("canned-shader strategy settled no later
than F4"); ADR 0015 (fixed-function is a consumer contract); ADR 0021
(single-source sokol-shdc shaders).

## Context

F3 shipped an unlit mesh fill: `shaders/mesh.glsl` multiplied vertex color
by tint, with the normal/uv attributes bound but unread. F4a is the first
milestone that consumes normals and the deferred decision the roadmap left
open — single mega-shader versus build-time shader permutations — had to be
settled here. F4a has no maps and no alpha masks, so every surface uses the
same shader with different uniform values; F4b's maps are the first thing
that could plausibly need a variant. The full process record is
`openspec/changes/f4a-lighting-phong/`.

## Decision

- **One lit mesh shader, driven by uniforms** (`shaders/mesh.glsl`,
  regenerated with the pinned sokol-shdc per ADR 0021). The shader is the
  same for every surface and material; per-surface Phong channels, the draw
  tint, the camera position, and the fixed light bank are uniforms. F4a
  introduces **no** per-material or per-feature shader permutation. If F4b's
  per-channel maps need one, that is F4b's decision and it arrives through
  the same single GLSL source.
- **Lighting is evaluated per fragment in world space** (Blinn-Phong): `N`
  is the interpolated, normalized world normal; `V` is the direction to the
  camera; the albedo is vertex color × `drawMesh` tint; the result is
  `ambient·albedo + emissive + Σ(diffuse·albedo·N·L + specular·(N·H)^shininess)·lightColor·atten`,
  clamped to `[0,1]` per channel, with no HDR/tonemapping. Emissive is added
  unmodulated by the albedo; specular is not modulated by the albedo.
- **Light semantics.** `dir` on the directional light is the direction the
  light *travels*; point lights attenuate linearly to zero at `range`
  (`atten = range == 0 ? 1 : clamp(1 - d/range, 0, 1)`). A surface with no
  `normals` uses F3's `(0,0,1)` object-space default. An unbound surface
  uses the engine default material: white diffuse Phong (ambient/emissive/
  specular black, `shininess` 32).
- **Mesh records snapshot the light bank** (ADR 0019): lights are value
  state, so a `drawMesh` record stays self-contained and re-orderable. The
  record budget is charged by `sizeof(efx_record)`; it was raised in the same
  change so the documented quad capacity is preserved.

## Consequences

- F4b/F5 must respect the uniform-driven single shader: a map feature is an
  additive uniform/permutation decision inside this source, never a second
  consumer-visible shader surface (ADR 0015).
- Materials bind per surface and are snapshotted engine-side; a surface's
  shader inputs are read from the Mesh at playback, so there is no global
  material state (ADR 0024).
- World-space evaluation makes the CPU reference trivial to transcribe from
  the same spec text and keeps lights independent of the camera.
- The light snapshot grows every mesh record; the byte-based record budget
  is the knob that absorbs it.

## Rejected alternatives

- **Build-time shader permutations per material feature set**: rejected for
  F4a — there is nothing to permute yet (no maps, no masks), and the
  pipeline-selection machinery would be built before a feature needs it.
  Deferred to F4b if its maps require it.
- **Keep the unlit shader for unbound surfaces and select a lit shader per
  surface**: rejected — it contradicts the settled "default material = white
  diffuse Phong" contract and doubles the pipeline surface for no F4a need.
- **Object-space or view-space lighting**: rejected — object space needs an
  inverse model to place lights and breaks under scaling; view space couples
  lighting to the camera and complicates the CPU reference. World space is
  the least surprising and mirrors directly on the CPU.
- **Transform normals by the model 3×3 instead of a normal matrix**:
  rejected — wrong under the non-uniform scale `drawMesh` permits.
- **Reference a global light buffer at playback instead of snapshotting**:
  rejected — violates the value-snapshot display-list contract (ADR 0019)
  and makes records order-dependent.
