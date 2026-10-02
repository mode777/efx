# Design

## Context

See `proposal.md` — Why. Current documentation state and the constraints that
shape the rewrite:

- `README.md` (135 lines) is the only top-level landing document. It opens with
  an F1/F2 milestone status line, lists F2 deliverables, then spends the
  majority of its length on the repository layout, the from-source build
  matrix, the ctest/golden harness, golden regeneration, and the CI gate.
- The engine is complete through F14. User-facing surfaces already exist that
  the README never mentions: the `gallery/` sample site (deployed by
  `.github/workflows/pages.yml`), the generated per-symbol reference in
  `docs/api/` (ADR 0048), the design guidelines in `docs/js-api.md`, runnable
  samples in `examples/` (`hello`, `hooks`, `browser`) and
  `gallery/samples/curated/`.
- The `efx` entry contract is stable and documented: a resource root (folder or
  zip) with `main.js`, `update`/`render` hooks, and CommonJS modules (ADR 0016,
  ADR 0037).
- There is no `CONTRIBUTING.md` today, and no `LICENSE` file (so no license
  claim can be made).
- Authoritative status and the codebase map already live in `AGENTS.md` and
  `openspec/specs/feature-roadmap`; duplicating them in the README is the
  problem being fixed.

## Goals / Non-Goals

**Goals:**

- Make the README a self-contained onboarding path for a developer who wants to
  build a game with EmotionFX and has never seen the repo.
- Preserve every piece of maintainer/CI knowledge by relocating it to
  `CONTRIBUTING.md`, not deleting it.
- Keep exactly one home for each fact: user-facing orientation in the README,
  build/verify/contribute detail in `CONTRIBUTING.md`, agent/tooling invariants
  in `AGENTS.md`, API detail in `docs/api/` + `docs/js-api.md`.

**Non-Goals:**

- No link checker, docs site, or automated spell/format enforcement.
- No rewrite of `docs/js-api.md`, `docs/api/`, or `AGENTS.md` beyond one
  pointer line.
- No license decision.

## Decisions

### D1 — Primary audience is the game/script developer

The README answers, in order: what is this, what can it do, how do I get it and
run it, how do I write my first script, where do I go deeper. "Running someone
else's packaged game" is not a separate path: a packaged game is a resource
root, so the same quick start (point the player at a resource root) covers it.

_Alternatives:_
- **Dual developer/player README** — players are served by the resource-root
  run path already; a second top-level track adds surface without a distinct
  workflow. Lost on simplicity.
- **Keep the status-log README and add a short intro on top** — the F1/F2
  framing and dev detail are precisely what make it feel outdated. Lost.

### D2 — Move maintainer content to `CONTRIBUTING.md` (don't delete)

The build matrix, headless build, ctest/golden harness, golden regeneration,
verification server, and CI gate are real and useful — for contributors, not
end users. A conventional `CONTRIBUTING.md` gives them a home and keeps the
README short.

_Alternatives:_
- **Keep a condensed build section in the README** — still mixes audiences and
  re-grows over time. Lost.
- **Drop the content and rely on `AGENTS.md`/`docs/`** — those are written for
  agents and for invariants, not for a human contributor's first build. Lost on
  discoverability.

### D3 — Fix the information architecture, keep prose

The README uses a fixed outline: title/tagline → what it is → capabilities →
get it (releases + browser gallery) → quick start (`main.js`) → project layout
→ run modes → API/next steps → contributing pointer. Sections are short and
link out rather than duplicate per-symbol API detail.

_Alternatives:_
- **Generate the README from another source** — no generator exists and the
  content is narrative; over-engineering. Lost.
- **Copy the API reference into the README** — duplicates the generated
  `docs/api/` and would drift. Lost.

### D4 — Resolve the gallery URL from the repo, don't assume it

The README needs a "try it in the browser" link. GitHub Pages is enabled for
this repo and `pages.yml` deploys the gallery, but the exact project URL must
be read from the repository's Pages configuration (e.g. `gh api
repos/mode777/emotion-fx/pages`) during implementation, failing back to the
repository Pages index rather than a guessed hostname. This follows the
no-guessed-URLs rule.

_Alternatives:_
- **Hardcode a `*.github.io` URL from memory** — risk of a wrong/moved
  hostname; rejected.
- **Omit the browser link entirely** — loses the strongest zero-install
  onboarding path. Lost.

### D5 — No ADR

The change records an audience split between two Markdown files. It settles no
cross-cutting technical invariant and constrains no future engine change, so it
does not meet the ADR bar. The rationale is captured here and in the documents
themselves.

_Alternatives:_
- **ADR for README vs CONTRIBUTING conventions** — a durable *why* future
  changes must respect is not at stake; the project's ADR bar is architecture.
  Lost.

### D6 — Resource exposure

Not applicable: this change introduces no script-visible resource type and no
native handle. The resource/memory model documentation is untouched (it lives in
the generated reference and `docs/js-api.md`).

## Risks / Trade-offs

- **Content loss during a full rewrite** → relocate rather than delete: every
  build/test/CI fact moves to `CONTRIBUTING.md`, and the task list calls out
  each block explicitly.
- **Drift between README quick start and the real entry contract** → the quick
  start mirrors `examples/hello/main.js`; verify the snippet runs with the
  built player.
- **Gallery URL wrong or Pages disabled at implementation time** → resolve via
  `gh api`, fall back to the repository Pages index, and keep the phrase
  generic ("sample gallery") if no URL is available.
- **Two docs become three homes for facts** → enforce one-home discipline in
  tasks: status/milestones stay out of the README; build/CI detail stays out of
  the README; API symbols stay out of both.
- **Over-long CONTRIBUTING** → acceptable; it is the maintainer entry point and
  mirrors the current README's dev sections.

## Migration Plan

1. Resolve the gallery Pages URL from the repository.
2. Draft `CONTRIBUTING.md` by moving the README's build/test/CI/verification
   sections into it, adding the codebase map and pointers to `AGENTS.md` and
   `docs/`.
3. Rewrite `README.md` around the end-user outline (D1/D3).
4. Add the one-line `CONTRIBUTING.md` pointer to the `AGENTS.md` Documentation
   section.
5. Verify: the quick-start snippet runs via the built player; all relative
   links resolve to existing files; no status/milestone claims remain in the
   README; no maintainer-only instructions remain in the README.

Rollback: both files are prose; reverting the commit restores the prior README
and removes `CONTRIBUTING.md`. No code, build, or gate impact.

## Open Questions

- Whether the repository should eventually gain a `LICENSE` file and README
  license section. Deferrable; explicitly out of scope here.
