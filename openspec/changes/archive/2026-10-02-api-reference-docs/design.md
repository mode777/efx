# Design

## Context

See `proposal.md` — Why. The current documentation state and the constraints
that shape the approach:

- `gallery/src/api/efx.d.ts` is the typed contract. It is loaded into the
  gallery's Monaco editor via `addExtraLib`, type-checked by `svelte-check`,
  and — after the recent TSDoc pass — carries summaries, `@param`,
  `@returns`, and `@example` for the whole surface. It has no `import`/`export`
  and declares a single ambient global `efx`.
- `docs/js-api.md` is a hand-written catalog that duplicates the declaration
  and has drifted from it.
- The gallery is a Vite + Svelte SPA (`base: './'`) built into `gallery/dist`
  and deployed by `.github/workflows/pages.yml` through
  `actions/upload-pages-artifact` + `actions/deploy-pages`. There is exactly
  one Pages deployment per repo, so the reference must ride in the same
  artifact.
- The four-target `ci.yml` gate does not build the gallery at all; it is
  unaffected by this change.
- Constraints: TypeScript is `6.0.3`; Node in Pages is 20; generated output
  must not be committed under `public/` (that tree is a mix of committed and
  gitignored inputs); the committed Markdown must stay reviewable.

## Goals / Non-Goals

**Goals:**

- Make `gallery/src/api/efx.d.ts` the single source of truth for the
  per-symbol reference, rendered by one pinned generator into committed
  Markdown (`docs/api/`) and pipeline-built HTML (`/api`).
- Make `docs/js-api.md` the durable design-guidelines document.
- Prevent the committed reference from silently going stale.
- Keep the reference visually part of the gallery (PS2 palette).

**Non-Goals:**

- No engine/runtime changes and no change to the four-target gate.
- No layer/milestone tags in the generated reference.
- No second site toolchain (SSG) and no custom TypeDoc renderer.
- No inclusion of narrative docs or ADRs in the generated reference.

## Decisions

### D1 — Generate from the TypeScript declaration (SSOT)

The declaration is already the machine-checked contract consumed by Monaco
and `tsc`, and it now carries the prose. Generating the reference from it
makes the editor hover, the committed Markdown, and the published HTML
inherently consistent.

_Alternatives:_
- **Keep the hand catalog** — already drifted; two sources to reconcile on
  every API change. Lost.
- **Generate from the C registration code** — would describe bindings, not
  the script contract (option bags, defaults, error semantics live in the
  binding layer and prose); no type model. Lost.
- **Author Markdown by hand and generate HTML from it** — still two sources
  and no compile-time check that the declaration and docs agree. Lost.

### D2 — TypeDoc + typedoc-plugin-markdown

TypeDoc is the canonical TSDoc consumer, supports the project's TypeScript
6.0.x, handles an ambient global `.d.ts`, renders `@example`, and ships its
own search/nav theme. The Markdown plugin emits one file per symbol with
working relative links.

_Alternatives:_
- **VitePress / Starlight + typedoc markdown** — a whole second site
  toolchain and build for a reference page that already has a good default
  theme. Lost on cost.
- **API Extractor + api-documenter** — aimed at npm-package APIs; awkward
  for a single ambient global and heavier. Lost.
- **`typedoc --json` + a custom Svelte renderer** — unifies the UI but makes
  us own a renderer against an unstable JSON schema, for no requirement the
  default theme doesn't already meet. Lost.

### D3 — Committed Markdown at `docs/api/`

One file per symbol plus a generated `README.md` index (105 files, ~460 KB)
under `docs/api/`. Use `--disableSources` so `Defined in: efx.d.ts:NNN` line
references do not churn on every declaration edit. Links are relative `.md`
paths that GitHub resolves.

_Alternatives:_
- **A single giant Markdown file** — unreviewable diffs and no per-symbol
  anchors. Lost.
- **`docs/reference/`** — `docs/api/` matches the published `/api` route and
  the existing `docs/` tree. Lost on naming coherence.

### D4 — HTML into `dist/api` after `vite build`

`vite build` empties `dist`, so the reference is generated after it into
`dist/api`. `vite preview` then serves the whole site (gallery at `/`,
reference at `/api`) locally. Generated output never touches the committed
`public/` tree.

_Alternatives:_
- **`public/api` before `vite build`** — dev-servable via `npm run dev`, but
  puts generated files in a tree that also holds committed inputs and needs
  another gitignore entry. Lost on cleanliness; `vite preview` covers local
  checking.

### D5 — Drift guard by regeneration + diff

A `docs:check` script regenerates the Markdown to a temp location and diffs
it against `docs/api/`, failing on any difference. `pages.yml` runs it before
building, so a stale reference blocks the deploy. This mirrors the existing
`tools/gen_prelude.py --check` pattern already used for the prelude.

_Alternatives:_
- **Rely on the AGENTS.md rule alone** — the rule is the process, but nothing
  enforces it. Lost.
- **A pre-commit hook** — not guaranteed on agents/CI and adds local setup.
  Lost.

### D6 — Repurpose `docs/js-api.md` as guidelines

Keep the path (many references) but change its role: it retains and expands
the rules humans must apply — namespace, two-layer structure, naming,
option-bag and validation conventions, units/colors, error model, resource &
memory model, fixed limits, lifecycle, module model, gamepad model — plus a
new "adding to the API" process, and points to the generated reference for
per-symbol detail. The per-function catalog sections are removed.

_Alternatives:_
- **Keep both catalog and reference** — the duplication this change exists to
  remove. Lost.
- **Rename to `docs/api-guidelines.md`** — churns references across
  AGENTS.md, config, specs, and ADRs for no behavioral gain. Lost.
- **Delete `docs/js-api.md`** — loses the design rules that are not
  per-symbol and cannot be generated. Lost.

### D7 — Drop layer tags from the reference

Layering is an implementation concern; the API is end-user facing. The
two-layer rule stays a guideline (D6). This removes the `js-api` spec's
per-entry layer-tag obligation.

_Alternatives:_
- **Add `@category C`/`@category JS` to the declaration** — carries
  implementation detail into user-facing docs and pollutes the editor hovers.
  Lost.
- **Keep a hand-maintained layer table in the guidelines** — optional; the
  layering rule is what matters, not an exhaustive classification. Deferred.

### D8 — PS2 theming via `--customCss`

The default theme is driven by CSS variables (`--color-accent`,
`--dark-color-background`, `--color-ts-interface`, …). A single committed
stylesheet overriding those with the gallery's dark/glow palette is enough.
Force the dark scheme so the reference matches the shell.

_Alternatives:_
- **A community TypeDoc theme** — extra dependency and a different look than
  the gallery. Lost.
- **A custom TypeDoc theme plugin** — far more work than a palette. Lost.

### D9 — Record the decision as an ADR

A new `docs/decisions/0042-api-reference-generated-from-type-doc.md` records
that the declaration is the SSOT for the reference, that `docs/api/` is
generated and never hand-edited, and that `docs/js-api.md` is guidelines. It
is cross-cutting (every API change must regenerate) and durable, so it meets
the ADR bar.

### D10 — Resource exposure

Not applicable: this change introduces no script-visible resource type and no
native handle. The resource/memory model and fixed-limits documentation move
verbatim into the guidelines (`docs/js-api.md`); no resource classification
changes.

## Risks / Trade-offs

- **Generated-file churn in diffs** → `--disableSources` removes line
  references; only genuine content changes show.
- **Generator/TypeScript version drift** → pin `typedoc` and
  `typedoc-plugin-markdown` exactly; the drift guard fails if output changes.
- **Two TypeDoc configs diverge** → share one base config and have the
  Markdown config extend it; the HTML config adds only theme/output.
- **Markdown links not rendering on GitHub** → the plugin emits relative
  `.md` links; verify the committed tree in a PR/branch before merge.
- **A stale reference only fails on `main` (Pages runs there)** → the
  AGENTS.md rule makes regeneration part of the change; optionally add the
  cheap `docs:check` to a CI job later. The guard still prevents a bad
  deploy.
- **Loss of narrative detail from `js-api.md`** → the resource model, fixed
  limits, lifecycle, and module/gamepad design rules are explicitly retained
  in the guidelines (D6), not dropped with the catalog.
- **`/api` collides with a future path** → the route is reserved by this
  change and documented in the spec; low risk.
- **PS2 palette reduces contrast/readability** → constrain the override to
  the accent/background variables and keep the theme's text contrast; verify
  light and dark.

## Migration Plan

1. Add pinned devDependencies and TypeDoc configs to `gallery/`.
2. Generate `docs/api/` and commit it; add the theme stylesheet.
3. Add `docs:markdown`, `docs:check`, and `docs:html` scripts.
4. Rewrite `docs/js-api.md` as guidelines (catalog removed, rules retained and
   expanded, pointer to the reference added).
5. Add the `docs:check` + `docs:html` steps and the `dist/api` existence
   check to `pages.yml`.
6. Update `AGENTS.md` and `openspec/config.yaml` rules; fix the two stale
   `docs/js-api.md` references in the declaration.
7. Write and index ADR 0042.

Rollback: the change is additive to the site (a new `/api` subtree) and to
docs; reverting the commit restores the previous state. The engine and gate
are untouched.

## Open Questions

- Whether to later add the narrative docs/ADRs into the reference via
  TypeDoc project documents. Deferrable; does not affect specs or tasks.
- Whether to add a custom favicon/custom JS flourish beyond the palette.
  Deferrable; palette satisfies the presentation requirement.
