# Proposal

## Why

The top-level `README.md` is the first thing a prospective user sees, but it
has not kept up with the product and is written for the wrong reader. It still
advertises "**F2 (2D layer) done**" and describes the repository as if the
project were mid-build: the feature list stops at the F2 deliverables, the
"Repository layout" is stale, and most of the document is maintainer material
(ctest, golden regeneration, the four-target CI gate, golden determinism).
None of it tells a game/script developer what EmotionFX is today, how to get
it, or how to write their first `main.js`. The engine is feature complete
through F14 (2D/3D, lighting, post-FX, glTF, skinning, text, input, modules,
particles, physics, gamepad, audio), ships a browser sample gallery and a
generated API reference, and has downloadable players — none of which the
README surfaces.

## What Changes

- **Rewrite `README.md` completely** for its primary reader, the **game/script
  developer**: what EmotionFX is, its capabilities, where to get the player
  (release downloads + try-in-browser gallery), a minimal quick start
  (`main.js` with `update`/`render`), the resource-root/CommonJS module model,
  a short tour of the `efx` API surface, run modes (windowed, `--script`,
  `--repl`), platform support, and pointers into the existing sample gallery,
  `examples/`, and the generated API reference.
- **Move all maintainer content out of the README into a new
  `CONTRIBUTING.md`**: the from-source build matrix, headless builds, the
  ctest smoke/golden harness, golden regeneration, the verification server, the
  CI gate, vendoring, and the codebase map.
- **Link the two documents**: the README gains a concise "Building from source
  / Contributing" pointer to `CONTRIBUTING.md`; `CONTRIBUTING.md` points back
  to `AGENTS.md` and `docs/` for agent/tooling conventions.
- Remove stale status/milestone claims and the F2-centric feature list from the
  README; the maintained status lives in `AGENTS.md` and
  `openspec/specs/feature-roadmap`, not in the landing page.

## Capabilities

### New Capabilities

- None. This change is documentation-only and alters no engine or
  script-visible behavior.

### Modified Capabilities

- None. No spec-level behavior changes, so no delta spec is written. The
  change sets `skip_specs: true` in its `.openspec.yaml`.

## Impact

- `README.md` — replaced end to end; no other file's behavior depends on its
  content.
- `CONTRIBUTING.md` — new file carrying the relocated maintainer content.
- `AGENTS.md` — its Documentation section gains a one-line pointer to
  `CONTRIBUTING.md` so the split is discoverable; no invariant changes.
- No source, build, test, gallery, or workflow changes; the four-target
  verification gate and the Pages deploy are unaffected. No `js-api` delta, no
  `efx.d.ts` change, no `docs/api/` regeneration, no golden re-baseline.

## Milestone

This change implements **no roadmap milestone**. It is a post-F14
documentation change layered on the completed F1–F14 surface; it changes no
engine behavior and therefore does not enter the milestone ladder. (The
proposal rule asks for the implemented milestone; there is deliberately none.)

## Docs & ADR

- **Affected docs:** `README.md` (rewritten for end users), new
  `CONTRIBUTING.md` (maintainer/build content relocated), and a one-line
  `AGENTS.md` Documentation pointer.
- **ADR:** none. This is a documentation restructure with no durable
  cross-cutting technical invariant. The README/CONTRIBUTING audience split is
  recorded here and in `CONTRIBUTING.md` itself; it does not meet the ADR bar.

## Non-goals

- **No engine, runtime, platform, API, or test changes.** No behavior changes
  of any kind; the four-target gate is not run for this change.
- **No new documentation tooling or site** (no SSG, no docs framework, no
  link checker). Only Markdown documents are authored.
- **No changes to `docs/` or the generated `docs/api/` reference** (that
  reference is auto-generated and already covers the API).
- **No license file or license claim added.** The repository has no `LICENSE`
  file; deciding licensing is a separate, out-of-scope question and the README
  will not assert one.
- **No hosted-URL guessing.** The gallery link is resolved from the repository
  Pages configuration during implementation, not hardcoded from memory.
