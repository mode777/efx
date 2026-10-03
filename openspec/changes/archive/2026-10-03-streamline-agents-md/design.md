# Design

## Context

AGENTS.md is loaded into every agent session in this repo; its only consumers
are agents (human contributors use `CONTRIBUTING.md`). It currently fuses
three jobs — rules, process, history — and the history third (~170 of 293
lines) must be hand-synced per change and has drifted (it narrates two
changes that are not yet archived). The milestone ladder it enforces ended
with F14; the file's process rules reference a world that no longer exists.
See proposal.md — Why.

Constraint from the docs layer: `openspec/specs/` pins behavior, `docs/`
holds invariants, and AGENTS.md "points rather than restates". The current
file violates its own dividing rule.

## Goals / Non-Goals

**Goals:**

- A fresh agent's first ~100 lines are entirely actionable: identity,
  lifecycle, constraints, map — zero "done" rows, zero run IDs.
- Every fact removed from AGENTS.md remains derivable from exactly one
  authoritative home (archive listing, ADR index, roadmap spec).
- A structural guard against re-bloat, not a one-time cleanup.

**Non-Goals:**

- Changing any product code, spec, ADR content, or CI workflow.
- Reformatting history that already lives in the archive — only AGENTS.md's
  duplication of it is deleted.

## Decisions

1. **Delete history from AGENTS.md outright; do not compress it.**
   The milestone index and roadmap table become a 3-line pointer block to
   `openspec/changes/archive/`, `docs/decisions/`, and
   `openspec/specs/feature-roadmap`.
   *Rejected: keeping a compacted one-line-per-milestone table* — it still
   requires hand-syncing per change, which is the failure mode being fixed;
   any retained table regrows rows.

2. **Supersession facts migrate to the ADR index Status column.**
   The milestone index carried one load-bearing signal the ADR index lacks:
   which ADRs define *current* behavior where a later decision replaced an
   earlier one (F8b → ADR 0024; music/effect audio split → ADR 0047; pre-GPU
   upload queue → ADR 0052). The index already supports this — rows 0009 and
   0013 read "Superseded by …". Annotate the three rows; AGENTS.md drops the
   facts.
   *Rejected: a separate "ADRs that define current behavior" doc* — a third
   place to sync; the Status column keeps the signal adjacent to the decision
   it qualifies.

3. **The maintenance rule lives in AGENTS.md itself.**
   "AGENTS.md records invariants and pointers, never per-change history;
   anything derivable from the archive, the ADR index, or the roadmap spec
   does not belong here."
   *Rejected: relying on reviewer discipline alone* — the file bloated under
   exactly that regime; a written rule gives reviewers something to enforce.

4. **One numbered change lifecycle replaces six bold bullets.**
   Propose → apply → server verification (`tools/verify_remote.py`, env-only
   SSH creds) → gate dispatch (`gh workflow run ci.yml`, Linux → Windows →
   macOS, fix-forward between stages) → merge to `main` on green → archive.
   Every current rule survives verbatim in meaning; only the presentation
   changes.
   *Rejected: keeping the bullets* — the order is the content; bullets force
   each reader to re-derive the sequence.

5. **Dated "Open risks" section replaces facts buried mid-bullet.**
   Each item carries an expiry date and is pruned when stale (first entry:
   ubuntu-latest → Ubuntu 26 on 2026-10-19 may bump llvmpipe and require a
   golden re-baseline per ADR 0020).
   *Rejected: an "open questions" list without dates* — undated temporary
   content becomes permanent content.

6. **`openspec/config.yaml` gets a surgical staleness fix, not a rewrite.**
   The `context` block still presents the F1–F8 ladder as live; the
   `rules.proposal` entry still demands a milestone name; the `rules.tasks`
   entry ties verification to milestone gates. Replace those three with the
   post-roadmap convention (name the capability spec(s); verification names
   the applicable harness). Keep all other rules and the YAML shape
   byte-compatible where possible — the CLI already emits a rules-parse
   warning; do not give it new excuses.
   *Rejected: leaving config.yaml alone* — it is injected into every future
   artifact creation in this repo; fixing AGENTS.md while the config still
   issues milestone instructions defeats the purpose.

7. **Target structure (~100 lines), in order:** What this is → Change
   lifecycle → Non-negotiable design constraints (kept nearly verbatim — the
   strongest section) → Repo map (pointer rows; the `docs/api/` auto-
   generated warning appears exactly once) → Open risks → Maintenance rule.
   Stack facts fold into "What this is" at one line each.

## Risks / Trade-offs

- [Deletion loses a fact someone needed] → Nothing leaves the repo: archive,
  ADR files, and roadmap spec are untouched; the three supersession facts
  move to the index before AGENTS.md drops them. Diff review gates the merge.
- [Agents re-add history bullets out of habit] → The maintenance rule is a
  top-level section, and reviewers can reject a PR that adds per-change state
  by citing it.
- [Editing config.yaml rules could trip the known CLI parse warning] → Keep
  the existing YAML structure and list-of-strings shape; strings-only edits.
  The warning is pre-existing and non-fatal either way.
- [Verification guidance gets stale as tooling changes] → AGENTS.md keeps
  commands only where they are stable contracts (verify_remote.py, gh
  workflow run); deep detail stays in `docs/verification-server.md` and
  `CONTRIBUTING.md`, which it points to.

## Migration Plan

Single docs commit on a branch: rewrite AGENTS.md, annotate three ADR index
rows, patch config.yaml context/rules. Verify with `npx openspec validate
--strict` and a human diff read; no ctest applies (no code). Rollback is
`git revert` of one commit. Server/gate verification is not required by the
four-target gate for docs-only changes, but the standard branch → verify →
dispatch flow remains available and harmless if desired.

## Open Questions

None — the one material tension (where supersession facts live) is settled
in Decision 2.
