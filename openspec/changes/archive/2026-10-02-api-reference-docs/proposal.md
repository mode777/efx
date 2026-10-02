# Proposal

## Why

The script-facing API is currently described in two hand-maintained places:
`gallery/src/api/efx.d.ts` (the typed contract, loaded into the gallery editor
and now carrying full TSDoc with params/returns/examples) and
`docs/js-api.md` (a per-symbol catalog). The catalog duplicates the
declaration, and the two have already started to diverge (e.g. the reference
omitted `drawBillboard`'s `plane` facing mode and several option details). Now
that the declaration is thoroughly documented, the per-symbol reference can be
generated from it, and `docs/js-api.md` can return to what only humans can
maintain: the rules for designing new API.

## What Changes

- Generate a per-symbol API reference from `gallery/src/api/efx.d.ts` with
  TypeDoc (pinned; it supports the project's TypeScript 6.0.x).
- Commit the **Markdown** rendering to `docs/api/` so it is readable and
  reviewable on GitHub and in diffs.
- Build the **HTML** rendering in the Pages pipeline and publish it under
  `/api` on the gallery site, themed with the PS2 palette.
- Add a **drift guard** that regenerates the Markdown and fails if the
  committed `docs/api/` is stale (same pattern as
  `tools/gen_prelude.py --check`).
- Repurpose `docs/js-api.md` from a full catalog into **API design
  guidelines** (namespace/layering rules, option-bag conventions, units,
  colors, error model, resource & memory model, fixed limits, and the process
  for adding API).
- Add an **AGENTS.md** rule requiring the committed Markdown reference to be
  regenerated from the declaration in the same change as any API delta.
- **BREAKING (docs/process):** `docs/js-api.md` no longer contains the full
  per-symbol catalog; readers are directed to the generated reference.

## Capabilities

### New Capabilities

_None._ The reference and its publication fit the existing `js-api` and
`web-gallery` capabilities rather than introducing a near-duplicate one.

### Modified Capabilities

- `js-api`: replace the "Normative API reference document" requirement (a
  hand catalog tagging every function by layer and milestone) with a
  generated-reference model — the declaration is the single source of truth,
  `docs/api/` is its committed Markdown rendering, and `docs/js-api.md` holds
  design guidelines. Remove the per-entry layer-tag and milestone-tag
  requirements; keep the resource-classification, fixed-limits, lifecycle,
  module-model, and gamepad documentation obligations, now satisfied by the
  generated reference plus the guidelines.
- `web-gallery`: the static site build includes the generated API reference
  under `/api`, and the PS2 presentation applies to the reference as well as
  the gallery shell.

## Impact

- `gallery/`: new devDependencies (`typedoc`, `typedoc-plugin-markdown`), new
  scripts (`docs:markdown`, `docs:check`, `docs:html`), two TypeDoc configs, a
  committed theme stylesheet, and a drift-check script.
- New committed generated tree `docs/api/` (105 Markdown files, ~460 KB).
- `docs/js-api.md` rewritten; two stale `docs/js-api.md` references inside
  `gallery/src/api/efx.d.ts` removed.
- `AGENTS.md` and `openspec/config.yaml` rules updated.
- `.github/workflows/pages.yml` runs the drift check and builds the HTML
  reference into the deployed artifact.
- No engine, runtime, or platform code changes; the four-target verification
  gate is unaffected.

## Milestone

This change implements **no roadmap milestone**. It is a documentation and
tooling change layered on the existing F1–F13 API surface; it does not alter
engine behavior and therefore does not enter the milestone ladder. (The
proposal rule asks for the implemented milestone; there is deliberately none.)

## Docs & ADR

- **Affected docs:** `docs/js-api.md` (repurposed to guidelines), new
  `docs/api/` (generated reference), `AGENTS.md`, `openspec/config.yaml`.
- **ADR:** a new ADR is warranted —
  `docs/decisions/0042-api-reference-generated-from-type-doc.md` — recording
  that the declaration is the single source of truth for the per-symbol
  reference, that `docs/api/` is generated (never hand-edited), and that
  `docs/js-api.md` is the design-guidelines document.

## Non-goals

- No change to any script-facing API, engine behavior, or the four-target
  verification gate.
- No per-entry layer tags or milestone tags in the generated reference
  (milestones are internal; the API is end-user facing).
- No second documentation site or SSG (VitePress/Starlight); TypeDoc's own
  theme is used.
- No inclusion of the narrative docs or ADRs inside the generated reference
  (they remain separate Markdown under `docs/`).
- No automated publishing of the reference anywhere other than the existing
  GitHub Pages site.
