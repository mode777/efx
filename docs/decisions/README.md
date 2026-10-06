# Architecture decisions

Short, numbered records of durable architecture decisions — the **why**
behind invariants that span multiple modules, formats, or milestones.
Behavior requirements live in `openspec/specs/`; the per-change process
records (proposals, full design docs) live in `openspec/changes/`; this
directory holds only what stays true after a change is archived.

## Index

| # | Status | Decision |
| - | ------ | -------- |
| [0001](0001-c11-core-with-a-c-abi.md) | Accepted | C11 core with a C ABI for the script-facing API |
| [0002](0002-quickjs-ng-as-the-es6-runtime.md) | Accepted | quickjs-ng is the embedded ES6 runtime, without quickjs-libc |
| [0003](0003-module-walls-around-sokol-and-quickjs.md) | Accepted | Sokol and quickjs live behind module walls (platform/runtime/api/player) |
| [0004](0004-single-efx-global-namespace.md) | Accepted | Every engine function hangs off one global `efx` namespace |
| [0005](0005-glm-behind-a-plain-c-api.md) | Accepted | GLM is the math library, wrapped behind a plain C API (first use F3) |
| [0006](0006-vendored-pinned-source-snapshots.md) | Accepted | Dependencies are pinned source snapshots vendored in-repo |
| [0007](0007-headless-script-mode-exit-codes.md) | Accepted | Player run modes with an exit-code contract (resource-root, `--script`, `--repl`) |
| [0008](0008-node-as-test-launcher-only.md) | Accepted | Node is a test launcher, never a script dependency |
| [0009](0009-github-actions-gate-runner.md) | Superseded by [0023](0023-tag-triggered-ci-and-releases.md) (trigger policy) | GitHub Actions is the four-target gate runner |
| [0010](0010-script-math-is-plain-js-data.md) | Accepted | Script math is plain JS data; GLM math stays behind the C wall |
| [0011](0011-dynamic-resources-are-gc-finalized-classes.md) | Accepted | Dynamic-count resources are GC-finalized opaque classes; slots only for fixed banks |
| [0012](0012-native-memory-gc-discipline.md) | Accepted | Native memory counts toward GC pressure; destroy-first, finalizer-backstop discipline |
| [0013](0013-resource-taxonomy-eight-opaque-types.md) | Superseded by 0014 | Resource taxonomy: eight GC-finalized opaque types |
| [0014](0014-skinning-follows-gltf-data-model.md) | Accepted | Skinning follows the glTF data model; seven resource types (weights in MeshData, Skeleton ≈ glTF skin) |
| [0015](0015-fixed-function-is-consumer-api-contract.md) | Accepted | Fixed-function is a consumer-API contract; internals use Sokol's programmable pipeline with canned shaders |
| [0016](0016-explicit-hook-registration-implicit-init.md) | Accepted (amended 2026-10-02: the surface exists before evaluation in surface-bearing modes) | Lifecycle via explicit stacking hook registration; loading main.js is the implicit init |
| [0017](0017-implicit-rig-payload-skinned-flag.md) | Accepted | Skins and skeletons are implicit Mesh payload; `skinned` is a drawMesh flag (5 resource types) |
| [0018](0018-script-driven-posing.md) | Accepted | Script-driven posing via `poseMesh`; no engine playback state |
| [0019](0019-display-list-recording-semantics.md) | Accepted | Display list records are frame-transient; value-snapshot small state, handle-reference resources |
| [0020](0020-golden-image-verification.md) | Accepted | Golden images: PNG via vendored stb, ±2/255 tolerance with a 0.5% pixel exemption, software rasterizers and pinned toolchains in CI |
| [0021](0021-sokol-shdc-canned-shaders.md) | Accepted | Canned shaders come from sokol-shdc; one GLSL source, no hand-written per-backend flavors |
| [0022](0022-quickjs-desktop-only.md) | Accepted | quickjs never ships to the browser; the browser's native JS engine is the web runtime behind the `src/web/` bridge |
| [0023](0023-tag-triggered-ci-and-releases.md) | Accepted | CI runs on `v*` tags and manual dispatch only, and every run publishes four downloadable target archives (tag runs attach them to the release) |
| [0024](0024-multi-surface-meshes.md) | Accepted | Meshes are multi-surface (Godot-style, 1..16); materials bind per surface; the global `setMaterial` never ships |
| [0025](0025-engine-owned-clip-depth-remap.md) | Accepted | The engine folds the GL→0..1 clip-depth remap into the MVP at playback on `origin_top_left` backends; sokol normalizes depth state, not depth range, nor attachment formats |
| [0026](0026-lighting-and-canned-shader-strategy.md) | Accepted | F4a lighting is world-space Phong from one uniform-driven mesh shader (no permutations); lights are value state snapshotted per mesh record |
| [0027](0027-mesh-material-maps-and-alpha-mask.md) | Accepted | F4b maps are five always-bound samplers with a white fallback (no permutations); the alpha mask is a binary `alpha < 0.5` cutout; a bound map retains its Texture until unbound |

| [0028](0028-render-targets-sampled-directly.md) | Accepted | F5a render targets are sampled directly wherever a Texture is (no alias object), segmented in the display list, and released on the Texture lifecycle |

| [0029](0029-post-chain-architecture.md) | Accepted | F5b post-processing is one declarative chain over an implicit scene target (fast path preserved) with an engine-owned, non-script-visible effect registry and per-entry `mix` |
| [0030](0030-web-gallery-iframe-embedding.md) | Accepted | The sample gallery runs one engine instance per run in an iframe, fed by a host-only entry-source channel consumed before evaluation (`__efx_main_js`); no live eval/reset API |
| [0031](0031-resource-loading-provider.md) | Accepted | F6a resources load through one pure-C dir/zip provider; the web fetches a single zip once at boot, then the script-facing `load*` API stays synchronous everywhere |
| [0032](0032-gltf-import-profile.md) | Accepted | F6b pins the glTF 2.0 static-import profile: cgltf, provider-resolved `.glb`/`.gltf` references, one mesh (no scene graph), a fixed PBR→Phong mapping, per-texture samplers, and a fail-on-`extensionsRequired` policy |
| [0033](0033-gltf-rig-payload.md) | Accepted | F6c imports glTF skins/clips as CPU-only per-surface joints/weights plus an opaque MeshData→Mesh rig payload (LINEAR/STEP exact, CUBICSPLINE→LINEAR); no script rig API |
| [0034](0034-texture-mipmaps.md) | Accepted | F6e adds an opt-in `mipmaps` texture option: a deterministic CPU 2×2 box-filter chain uploaded in one image, with the sampler's `mipmap_filter` driven by `filter`; removes the `loadTexture` convenience |
| [0035](0035-cpu-skinning-pipeline.md) | Accepted | F7 poses imported rigs on the CPU: bind-locals reconstructed from inverse bind matrices, joint-space FK + skin-matrix palette, per-vertex weight normalization, and dual bind/posed vertex buffers selected by the `skinned` draw flag |
| [0036](0036-input-c-owned-frame-staged.md) | Accepted | F9 input is C-owned and frame-staged: one pure-C core feeds both bindings, queries plus unsubscribe-returning event callbacks, surface-pixel coordinates, and a test-only injection seam |
| [0037](0037-commonjs-module-format.md) | Accepted | F10 makes CommonJS the engine module format: synchronous `require` resolved through the dir/zip provider by one shared pure-JS runtime, with a restricted resolver; ESM is a source format compiled to CommonJS and npm/Node compatibility is a non-goal |
| [0038](0038-native-font-typesetting.md) | Accepted (the former F8b slice is retired, superseded by [0024](0024-multi-surface-meshes.md)) | F8a text is mid-level C: `loadFontData`→`createFont` bakes a fixed RGBA8 glyph atlas (optional baked outline/shadow) and `drawText`/`measureText` record opaque 2D quads; vendored stb_truetype/stb_rect_pack, no rich text/3D text (supersedes ADR 0013's font clause) |
| [0039](0039-cpu-particle-billboard-pipeline.md) | Accepted | F11 particles are CPU-simulated native-backed systems; one oriented-quad basis serves `drawBillboard` and particle `facing` (`view`/`y`/`plane`) over a new depth-test/no-write pipeline variant; `drawSprites` reuses quad records; particles sort within their batch; curated showcases accompany the golden |
| [0040](0040-physics-core.md) | Accepted | F12 collision/dynamics is a bespoke, dependency-free C11 core (its own math, no GLM) with one engine-owned world stepped by the script, a linear-only sequential-impulse solver, closest-feature narrowphase + conservative-advancement sweeps, one-way kinematic character push, sensors as a flag, and rounded cross-runtime determinism |
| [0041](0041-gamepad-input.md) | Accepted | F13 gamepad input is a vendored poll backend behind a pure-C fixed pad bank + SDL-mapping evaluator, sampled at frame begin, with canonical ranges/threshold, raw fallback, and no Asyncify |
| [0042](0042-audio-mixing-and-vendoring.md) | Superseded by [0047](0047-audio-source-model.md) (music/effect split) | F14 audio is push-mode mixing (no threads/atomics) over a vendored sokol_audio + dr_libs stack, with a fixed 32-voice bank, one streamed music source, decoded-PCM-only, and no-device soft-fail |
| [0043](0043-web-pointer-focus-default.md) | Accepted | On the web, pointer input must not suppress the focus default keyboard delivery depends on: the platform bubbles mouse events so a click focuses an iframe embed, while key/wheel default suppression stays |
| [0044](0044-curated-sample-dirs.md) | Accepted | A curated gallery sample is a self-contained resource-root directory; its gallery mount pack and the release `samples.zip` are derived from it by one deterministic pure-Node packer, with generated assets committed loose and drift-checked |
| [0045](0045-physics-substepping.md) | Accepted | `efx.physics.step` sub-divides the clamped `dt` into equal substeps no larger than `EFX_PHYS_MAX_SUBSTEP` (1/60 s) so a large frame delta cannot skip thin static geometry; per-call and bounded, the 1/60 path stays bit-identical, force acts over the whole step (amends ADR 0040's "no fixed-step accumulator" clause) |
| [0046](0046-physics-world-holds-live-bodies.md) | Accepted | The physics world holds every live `Body`/`Character` wrapper until `destroy()`, `physics.clear()`, or teardown, so GC never removes a collider from the simulation (amends ADR 0011/0012 for F12; the actual cause of the desktop fall-through bug misattributed in ADR 0045) |
| [0047](0047-audio-source-model.md) | Accepted | F14 audio is two source kinds (`AudioData` static, `AudioStream` streamed) loaded separately from playback over one `playAudio` verb returning an `Audio` handle; start options are initial values only, a single master gain is the only grouping control (no channels/buses), and the fixed 32-voice bank caps concurrent streams at 4 (supersedes ADR 0042's music/effect script surface) |
| [0048](0048-api-reference-generated-from-type-doc.md) | Accepted | The API reference is generated from `gallery/src/api/efx.d.ts` into committed Markdown (`docs/api/`) and published HTML (`/api`); `docs/js-api.md` is design guidelines, not a catalog |
| [0049](0049-shared-option-validation.md) | Accepted | Cold-path option-bag validation is written once in the shared prelude behind a private `natives` wrapper parameter (hot paths stay native); strict numbers everywhere; one return-code → error table; the 18 catalog message divergences converge to one canonical text each |
| [0050](0050-graphics-namespace.md) | Accepted | The 33 graphics drawing/state/resource functions live in the `efx.graphics` sub-namespace (bindings create the object, prelude augments it — the audio/physics pattern); the `efx` root holds only runtime/lifecycle facilities plus domain sub-namespaces; hard cut with no aliases, byte-identical error text, and new domains follow the same organization rule |
| [0051](0051-namespace-consolidation.md) | Accepted | The remaining root helpers move into sub-namespaces: math → `efx.math`, loaders → `efx.io` (plus `loadData` returning a `Uint8Array` copy), `whiteTexture` → `efx.graphics`, a frozen constants-only `efx.color` namespace (CSS basic 16 + transparent), and `args()` becomes the read-only `efx.args` property; completes ADR 0050's rule |
| [0052](0052-surface-less-resources-are-cpu-only.md) | Accepted | Surface-less run modes create CPU-only resources through the one creation path (`native = NULL`, no pixel copy); the deferred pre-GPU upload queue is removed and `whiteTexture` works everywhere (supersedes ADR 0034's queue clause) |
| [0053](0053-required-args-positional-options-bag.md) | Accepted | Required inputs are positional arguments and an options bag holds only optional configuration; a lone optional MAY stay positional, records keep required fields, argument order is subject → resources → scalars/vectors → bag; ten bag-buried functions move to positional requireds and `drawQuad`/`drawBillboard` lead with the thing drawn |
| [0054](0054-blend-mode-state-and-overrides.md) | Accepted | Blend is frame-local render state (`setBlendMode` resets to `alpha` each frame) with per-object overrides (`DrawQuadOptions`/`DrawSpritesOptions`/`DrawBillboardOptions.blend`, per-surface `Material.blend`); mesh surfaces resolve their blend at record time, particles inherit the frame state when unconfigured |
| [0055](0055-resource-operation-methods.md) | Accepted | An operation whose subject is a native-backed class instance is a method on that class (`Font.measure`, `Mesh.pose`, `Mesh.setSurfaceMaterial`); `efx.graphics` holds constructors/factories and stateless operations and does not re-take a class instance as a free-function argument (hard cut, no aliases) |
| [0056](0056-dropped-resource-roots.md) | Accepted (desktop restart mechanism superseded by [0057](0057-desktop-in-place-game-swap.md)) | A dropped resource root is a host-level feature (no script API; native zip/folder, web zip) loaded by restarting the run |
| [0057](0057-desktop-in-place-game-swap.md) | Accepted | A dropped root swaps the desktop game in place: a stable player session, a frame-start swap, and a render reset/pipeline rebind that keep the window and sg context alive (supersedes ADR 0056's desktop relaunch) |

## Adding a decision

Copy [`TEMPLATE.md`](TEMPLATE.md) to `NNNN-short-slug.md`, fill it in,
and add a row to the index. Keep it short — context, decision,
consequences, rejected alternatives. Do not restate behavior that
`openspec/specs/` already pins down; link instead. Supersede by marking
the old entry Superseded and linking forward; do not delete accepted
records.
