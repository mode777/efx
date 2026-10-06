# Design

## Context

See `proposal.md` — Why. The current pipeline is a single entry point,
`gallery/src/api/efx.d.ts`, rendered twice by pinned TypeDoc 0.28 +
`typedoc-plugin-markdown` 4.13.1: Markdown into the committed `docs/api/`
(drift-guarded by `gallery/scripts/check-docs.mjs`), and HTML into the
gallery's `dist/api`. Both share `gallery/typedoc.json`; the two renderings
differ only by the `markdown`/`html` config extensions.

Two facts constrain the approach:

- The declaration is the **single source of truth**; TSDoc is the only
  place per-symbol prose lives. Any editorial metadata (grouping, hiding)
  must therefore be expressed as TSDoc tags on the declaration itself.
- The committed Markdown is **generated and drift-checked**, so every input
  to generation (tags, config, the landing source) must be deterministic
  and committed.

Probes against a scratch declaration confirmed the exact tag semantics this
design relies on: `@group` + `groupOrder` replaces kind-based index grouping;
`@hidden` drops a symbol from the index and page set; `@inline` expands a
type's fields at each use site; and `@hidden @inline` together removes the
page while keeping the fields reachable inline. `indexFormat: "table"`
renders each group as a name/description table.

## Goals / Non-Goals

**Goals:**

- A domain-first reference whose index leads with `efx` and its
  sub-namespaces and never shows reflection-kind headings.
- Configuration option types that no longer occupy index entries while
  remaining fully documented at the operations that accept them.
- One shared TSDoc tag taxonomy that improves both renderings identically.
- A short LÖVE-style landing page merged as the root of both renderings.

**Non-Goals:**

- No change to the declaration's types or signatures, and no change to any
  runtime behavior.
- No restructuring of `efx.d.ts` into namespaces/modules and no custom
  TypeDoc plugin or post-processor.
- No second copy of per-symbol prose: the landing page introduces the API,
  it does not catalog it.

## Decisions

### D1 — Tag-driven organization (chosen) over a custom plugin

Grouping and hiding are expressed as TSDoc tags on `efx.d.ts` plus a small
`typedoc.json` delta, not a plugin that infers groups from the `Efx`
property graph.

- *Why:* tags keep the single source of truth intact, need no new
  maintenance surface, and apply to both renderings automatically. A plugin
  is more code to own and test for a one-time information-architecture fix.
- *Alternatives considered:* a TypeDoc plugin that walks `Efx` and assigns
  each referenced interface to the sub-namespace that names it (rejected:
  indirection, harder to reason about, and it cannot express the
  option-vs-value distinction); splitting the declaration into `@module`
  files (rejected: large refactor that perturbs the type contract and the
  gallery's `?raw` consumption for a docs-only win).

### D2 — Group taxonomy and index format

Tag every top-level declaration with one `@group`, and set `groupOrder` in
the shared config. The ordered groups are:

```
Start Here · Graphics · Math · Input · Physics · Audio · IO
Values · Configuration · Events · Enums · Results · *
```

`IO` was added during apply so `EfxIo` (a sub-namespace the spec requires the
reference to lead with) has a domain of its own. `EfxColor` sits under
`Values`, next to the `Color` primitive.

`indexFormat: "table"` (a markdown-plugin option, so it lives in
`typedoc.markdown.json`) renders each group as a name/description table.
`@groupDescription` adds a one-line gloss under a heading where useful.

- *Why:* this is the direct analogue of LÖVE's module-first pages; a reader
  scans "Graphics" and "Physics" instead of "Interfaces / Type Aliases /
  Variables". Tables surface each symbol's one-line summary, which the flat
  list hides.
- *Alternatives considered:* `@category` (TypeDoc categories are orthogonal
  to groups and can be layered later; groups were chosen because they
  replace the default kind grouping rather than adding a second axis);
  `hideGroupHeadings` (rejected: it flattens everything and loses the
  domain structure we are trying to create).

### D3 — `efx` is the root surface page

Hide-and-inline the `Efx` interface (`@hidden @inline`) and move its summary
and the "Hello Cube" example onto the `efx` variable's TSDoc, so
`variables/efx.md` expands the whole surface — sub-namespace properties and
root methods — as the canonical entry point. `efx` carries `@group Start
Here`.

- *Why:* the spec requires the reference to lead with the `efx` global. The
  probe confirmed `@hidden @inline` on `Efx` removes `interfaces/Efx.md` and
  makes `variables/efx.md` render the object's type declaration inline, with
  links to `EfxGraphics`, `EfxMath`, … intact.
- *Alternatives considered:* keep `Efx.md` and hide the `efx` variable
  (rejected: the root page would then be titled "Interface: Efx", not the
  literal `efx` the reader types); keep both (rejected: two pages for one
  object, and the duplicate `Efx` entry reappears in the index).

### D4 — Inline per-operation option bags; keep reused types as pages

Apply `@hidden @inline` to single-use, per-operation configuration
interfaces (the `*Options` bags), so their fields render inside the
operation that accepts them. Keep as grouped pages: reused value records
(`Material`, `SourceRect`, `PoseSample`, the shape descriptors,
`MeshSurfaceData`, `PhongChannel`, `SpecularChannel`, `FontOutline`,
`FontShadow`), event payloads, enums/aliases, result records, and
native-backed resource classes.

- *Why:* this mirrors LÖVE documenting options as parameters. A bag used by
  exactly one function has no independent identity; a record used by several
  signatures (e.g. `Material`, referenced by `CreateStaticMeshOptions` and
  `Mesh.setSurfaceMaterial`) would otherwise be duplicated in every page
  and lose its single canonical description.
- *Trade-off:* an option interface's own `@example` is not carried to the
  inlined use site (the probe confirmed only fields expand). Mitigation:
  examples for options live on the consuming operation's TSDoc, which the
  existing declaration already mostly does.

### D5 — Hide module-scoped authoring facilities

`@hidden` the CommonJS plumbing: the `EfxRequire`, `EfxModule`, and
`EfxModuleCacheEntry` interfaces and the `require`, `module`, `exports`,
`__filename`, `__dirname` variables.

- *Why:* `docs/js-api.md` states these are module-scoped authoring
  facilities, never members of `efx` and never globals; they are the six
  Variables currently polluting the index. They stay type-checkable (tags
  do not affect TypeScript) but leave the public reference.

### D6 — Landing page merged into both renderings

Author a short `gallery/src/api/README.md` and set `readme` to it in the
shared `typedoc.json` (replacing `"none"`), with `mergeReadme: true` in
`typedoc.markdown.json`. Content: what EFX is, the one `efx` global and its
sub-namespaces as a link table, the smallest complete script (Hello Cube),
and a pointer to `docs/js-api.md` guidelines.

- *Why:* `mergeReadme` appends the generated index to the readme so the
  committed `docs/api/README.md` becomes landing content + index — one root
  page, no second index to maintain. Setting `readme` in the shared config
  gives the HTML home page the same introduction.
- *Alternatives considered:* TypeDoc `@document` project documents
  (deferred: guides are out of scope; one landing page is enough);
  enriching only the `efx` TSDoc (rejected: a TSDoc comment is a weak
  landing page and cannot carry a link table or headings cleanly).

### D7 — Spec impact: a `js-api` delta, not `skip_specs`

The change alters the pinned "Normative API reference document" requirement
(it adds a hand-authored landing page to a "not hand-written prose"
reference and fixes its organization), so it carries a MODIFIED requirement
delta. No ADR: this is reference presentation, not a cross-cutting
architecture invariant; the durable rule is recorded in the `js-api` spec
and `docs/js-api.md`.

## Risks / Trade-offs

- **`@group` tags are verbose across ~110 declarations.** → Accepted; tags
  are inert to TypeScript and are the price of tag-driven. Method-level
  `@group` tags inside large interfaces (e.g. splitting `EfxGraphics` into
  Drawing / State / Resources / Text) are optional follow-up polish, not
  required by the spec.
- **Inlining a large option bag makes a function page long.** → Only
  single-use bags are inlined (D4); reused records stay pages, so no page
  accumulates the same block twice.
- **Hiding `Efx` breaks a link if a future symbol references it.** →
  `Efx` is referenced only by the `efx` variable today; add a note to
  `docs/js-api.md` that new symbols reference `efx`'s members, not `Efx`.
- **HTML grouping may not match the Markdown tables.** → Verify the built
  `dist/api` after regeneration; the tags are shared, so only the theme's
  rendering differs.
- **A future option interface is added without a tag**, silently reverting
  to a kind-grouped entry. → Add the taxonomy and the "tag new option bags
  `@hidden @inline`" rule to `docs/js-api.md`'s "Adding to the API" steps.

## Migration Plan

Single, docs-only change; no rollback concern beyond `git revert`.

1. Add the landing source and the config deltas.
2. Add tags to `efx.d.ts`; move the interface example to the `efx` variable.
3. Regenerate Markdown (`npm --prefix gallery run docs:markdown`) and commit
   `docs/api/`.
4. Build the HTML (`npm --prefix gallery run docs:html`) and eyeball
   `dist/api`.
5. `npm --prefix gallery run docs:check` must pass.

No four-target gate is required: no runtime code or script-facing behavior
changes.

## Open Questions

None blocking. Exact group membership for a few edge cases (e.g. whether
`EfxColor` sits under Graphics or Values) can be settled during apply
without changing the approach or the task breakdown.
