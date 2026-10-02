# Design

## Context

See `proposal.md` — Why. The engine core already uses the `efx` name (script
global, C targets `efx_core`/`efx_math`/`efx_platform`, `EFX_*` build knobs,
`efx.d.ts`); only a brand shell of `EmotionFX`/`emotion-fx`/`emotionfx`
remains. The live editing surface is small: ~30 hand-edited files. The rest of
the raw match count is noise:

- 114 generated `docs/api/**` files — regenerated, never hand-edited; the
  `docs:check` gate fails on drift.
- `src/prelude/prelude.h` — a byte-array embedding of `prelude.js`; the
  `gen_prelude.py --check` CI step fails on drift.
- 21 archived change records and the frozen baseline dumps under
  `openspec/changes/archive/2026-10-01-refactor-volume-build/baseline/**`
  (~2,600 matches) — historical, left untouched.
- One golden scene (`tests/goldens/text_basic`) whose source draws the old
  brand string, so its committed `golden.png` is pixel-affected.

Constraints that shape the approach: `docs/api/` and `prelude.h` are generated
and drift-gated; golden images require a display and are re-baselined on the
canonical Linux llvmpipe verification server (ADR 0020); the gate runs on tags
and manual dispatch only (ADR 0023); Pages deploys separately on pushes to
`main`; the project's OpenSpec flow expects planning artifacts before code.

## Goals / Non-Goals

**Goals:**

- Remove every live `EmotionFX` / `emotion-fx` / `emotionfx` reference from
  tracked, non-historical files.
- Rename the GitHub repository to `efx` and move the Pages site to
  `mode777.github.io/efx/` without leaving broken internal links.
- Keep behavior, the script-facing API, and every non-brand golden pixel
  identical; only `text_basic` changes, and only because its text string
  changed.

**Non-Goals:**

- Git-history rewrite; editing archived change records or frozen baselines.
- Renaming existing tags/releases or their already-attached assets.
- Renaming the `player` binary, CMake targets, or the `efx` namespace.
- Moving the local container workspace directory or the SSH server checkout
  (operational, outside the repo).

## Decisions

1. **Canonical names: display `EFX`, slug `efx`, CMake project `efx`; keep the
   `player` binary and `efx` namespace.** The internals are already `efx`, so
   this is the smallest change that removes the collision. Alternatives:
   "EFX Engine"/`efx-engine` (rejected — the request is just EFX, and the API
   global already matches); renaming the binary to `efx` (rejected — the
   resource-root run mode's binary name is `player` and is not part of the
   brand). The npm `package.json` name becomes `efx`; it exists only to
   install the OpenSpec CLI and is unpublished, so an npm name collision is
   immaterial — noted as a risk rather than solved with a scope.

2. **Leave historical records and frozen baselines untouched.** Archived
   changes are provenance, and the baseline dumps are large frozen snapshots;
   rewriting them is churn with no benefit. The sweep is scoped with explicit
   excludes (`openspec/changes/archive/**`).

3. **Regenerate, don't hand-edit, generated artifacts.** `docs/api/**` via
   `npm --prefix gallery run docs:markdown` (then `docs:check`), and
   `src/prelude/prelude.h` via `python3 tools/gen_prelude.py`; refresh
   `package-lock.json`. Both drift gates fail CI otherwise.

4. **Re-baseline only `text_basic`.** Capture its `golden.png` on the llvmpipe
   verification server per the AGENTS.md recipe. Every other golden must match
   byte-for-byte; a diff there is a regression, not a re-baseline.

5. **Rename the repository last, after the code/docs sweep is merged and the
   gate is green.** `gh repo rename efx` keeps an automatic redirect for the
   *repository* URL, but GitHub does not reliably redirect the *Pages* URL, so
   all in-repo links are updated in the same change and the new site is
   confirmed after the next `main` push. Doing the rename last avoids racing an
   in-flight gate run.

6. **No ADR.** This changes no architectural invariant or behavior; the
   durable conventions (single `efx` namespace, fixed-function pipeline) are
   unchanged. The brand name is recorded in the proposal, not an ADR.

7. **Post-roadmap housekeeping, no F# milestone.** Precedent:
   `efx-namespace-consolidation`. The proposal states this explicitly rather
   than forcing a milestone label.

## Risks / Trade-offs

- [Old Pages URL may 404 after rename] → update all internal links in this
  change, verify `mode777.github.io/efx/` after the next `main` push, and
  accept that external inbound links to the old project-site path may break
  (GitHub does not guarantee a Pages redirect). **Observed (2026-10-02):** the
  old `mode777.github.io/emotion-fx/` path returns 404 and is intentionally
  left broken; the new `/efx/` and `/efx/api/` paths return 200.
- [`docs:check` or `gen_prelude.py --check` fails on drift] → regenerate and
  commit both in the same change; the Linux gate job is the backstop.
- [`text_basic` golden mismatch on non-Linux targets] → re-baseline on the
  canonical llvmpipe capture, then run `verify_remote.py all` and the
  four-target gate; any *other* golden diff is treated as a regression.
- [CMake project rename invalidates existing build dirs] → reconfigure or
  clear local `build*` directories; they are gitignored.
- [Accidental edits to archived/frozen files] → scope every sweep with the
  `openspec/changes/archive/**` exclude and review `git grep` output.
- [Merge noise from in-flight changes] → land after `collapse-pre-gpu-queue`
  and `entry-after-gpu-init` are archived.
- [Repo rename during an active CI run] → perform the rename only after the
  verification gate is green and the branch is merged.

## Migration Plan

1. Sweep live files (display name, slug, CMake project, tooling default).
2. Regenerate `docs/api/**`, `prelude.h`, `package-lock.json`; run
   `docs:check` and `gen_prelude.py --check`.
3. Re-capture `tests/goldens/text_basic/golden.png` on the verification server.
4. `python3 tools/verify_remote.py all <branch>` → `gh workflow run ci.yml`
   (four-target gate).
5. Merge to `main` and push.
6. `gh repo rename efx`; update the local remote URL; confirm the Pages site
   redeploys at the new URL and the README links resolve.
7. Archive the change.

**Rollback**: revert the commit and, if the repo was already renamed,
`gh repo rename emotion-fx`. No data or behavior risk.

## Open Questions

- None that affect the approach. Moving the local workspace directory and the
  SSH server's `~/emotion-fx` checkout is deliberately deferred operational
  cleanup, not part of this change.
