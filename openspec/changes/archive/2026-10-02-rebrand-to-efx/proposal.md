# Proposal

## Why

The project currently ships under the name **EmotionFX**, which collides with
an existing commercial game middleware (EMotion FX, character-animation
middleware). Publishing under that name risks confusion and trademark
conflict. Meanwhile the engine's own internals already use `efx`: the
script-facing global is `efx`, the C targets are `efx_core`/`efx_math`/
`efx_platform`, the build knobs are `EFX_*`, and the type document is
`efx.d.ts`. "EmotionFX" is now only a leftover brand shell. Renaming to
**EFX** removes the collision and aligns the brand with the API.

## What Changes

- **Display name `EmotionFX` → `EFX`** across prose, titles, comments, and
  user-visible strings: README, CONTRIBUTING, `vision.md`, `docs/js-api.md`,
  `AGENTS.md`, the shader header comments, the prelude comment, the player
  window title, the REPL banner, and the gallery page titles/brand label.
- **Repository slug `emotion-fx` → `efx`**: rename the GitHub repository and
  update every in-repo reference — clone instructions, README/Pages links,
  `package.json`/`package-lock.json` metadata, and the `homepageUrl`.
- **Release/artifact names `emotion-fx-<version>-…` → `efx-<version>-…`** in
  the CI gate and the curated-samples packer. Existing tags/releases keep
  their old asset names; only future runs use the new names.
- **Build identity `emotionfx` → `efx`** for the CMake project name.
- **Verification tooling default remote checkout `~/emotion-fx` → `~/efx`**
  in `tools/verify_remote.py` and `docs/verification-server.md`.
- **Regenerate committed artifacts**: `docs/api/**` (from `efx.d.ts` +
  `typedoc.json`), `src/prelude/prelude.h` (from the edited `prelude.js`), and
  refresh `package-lock.json`.
- **Re-baseline the `text_basic` golden** (`tests/goldens/text_basic`), whose
  scene draws the old brand string. This changes only the committed golden
  image, not any behavior. **BREAKING** for the golden baseline only.
- **No script-facing API change.** The `efx` namespace is unchanged; no
  function, type, or runtime behavior changes.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- None. This change alters no spec-level behavior: no spec pins the product
  name, window title, artifact filename, or repository slug (verified by grep
  over `openspec/specs/`). It is a pure branding/docs/tooling migration, so it
  sets `skip_specs: true` in `.openspec.yaml` rather than inventing a
  requirement.

## Impact

- **Docs**: `README.md`, `CONTRIBUTING.md`, `vision.md`, `docs/js-api.md`,
  `docs/decisions/0040-physics-core.md`, `docs/decisions/0044-curated-sample-dirs.md`,
  `docs/verification-server.md`, `AGENTS.md`, `openspec/config.yaml`.
- **Code (strings/comments only)**: `src/platform/platform.c` (window title),
  `src/player/repl.c` (banner), `shaders/{quad,mesh,post}.glsl`,
  `src/prelude/prelude.js` (+ regenerated `src/prelude/prelude.h`).
- **Build/CI/tooling**: `CMakeLists.txt`, `.github/workflows/ci.yml`,
  `gallery/scripts/pack-samples.mjs`, `tools/verify_remote.py`.
- **Gallery**: `gallery/index.html`, `gallery/public/runner.html`,
  `gallery/src/lib/Frame.svelte`, `gallery/src/lib/SampleList.svelte`,
  `gallery/typedoc.json`, `gallery/package.json`, `gallery/vite.config.ts`.
- **Metadata**: `package.json`, `package-lock.json`.
- **Tests**: `tests/goldens/text_basic/main.js` + re-baselined `golden.png`.
- **Hosting**: GitHub repository rename and the resulting Pages URL move.

**Affected docs / ADR**: `docs/js-api.md` (title only — no design rule
changes), the generated `docs/api/` reference (regenerated), and the two ADR
prose mentions. **No ADR** — this settles no architectural invariant and
changes no behavior; it is a naming/branding migration, and the durable
conventions (one `efx` namespace, fixed-function pipeline, etc.) are unchanged.

**Roadmap**: implements no F1–F14 milestone — post-roadmap housekeeping
(precedent: `efx-namespace-consolidation`).

## Non-goals

- Do not rename the `player` binary, the CMake targets (`efx_core`,
  `efx_math`, `efx_platform`), or the `efx` script namespace — they are
  already correct.
- Do not rewrite git history, archived OpenSpec change records
  (`openspec/changes/archive/**`), or the frozen baseline dumps
  (`openspec/changes/archive/2026-10-01-refactor-volume-build/baseline/**`).
- Do not rename existing tags/releases or their already-attached assets.
- Do not rename the local container workspace directory or the SSH
  verification-server checkout as part of this change; the tooling default is
  updated, but moving the live directories is operational and done separately.
- No functional, API, or behavioral changes beyond the user-visible name
  strings.
