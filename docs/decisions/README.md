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
| [0016](0016-explicit-hook-registration-implicit-init.md) | Accepted | Lifecycle via explicit stacking hook registration; loading main.js is the implicit init |
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
| [0038](0038-native-font-typesetting.md) | Accepted | F8a text is mid-level C: `loadFontData`→`createFont` bakes a fixed RGBA8 glyph atlas (optional baked outline/shadow) and `drawText`/`measureText` record opaque 2D quads; vendored stb_truetype/stb_rect_pack, no rich text/3D text (supersedes ADR 0013's font clause) |
| [0039](0039-cpu-particle-billboard-pipeline.md) | Accepted | F11 particles are CPU-simulated native-backed systems; one oriented-quad basis serves `drawBillboard` and particle `facing` (`view`/`y`/`plane`) over a new depth-test/no-write pipeline variant; `drawSprites` reuses quad records; particles sort within their batch; curated showcases accompany the golden |

## Adding a decision

Copy [`TEMPLATE.md`](TEMPLATE.md) to `NNNN-short-slug.md`, fill it in,
and add a row to the index. Keep it short — context, decision,
consequences, rejected alternatives. Do not restate behavior that
`openspec/specs/` already pins down; link instead. Supersede by marking
the old entry Superseded and linking forward; do not delete accepted
records.
