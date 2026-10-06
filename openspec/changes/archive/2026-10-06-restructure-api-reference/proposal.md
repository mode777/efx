# Proposal

## Why

The generated per-symbol reference is a flat, kind-grouped, alphabetical
list. The `efx` global and its sub-namespaces (`graphics`, `math`, `io`,
`physics`, …) do not lead the page, and dozens of configuration option bags
sit in the index as peers of the public surface. A reader cannot discover
the API the way LÖVE's module-first documentation lets them: start at the
root object, pick a domain, then read the operations.

## What Changes

- **Make `efx` the entry.** Hide-and-inline the `Efx` interface so the
  `efx` variable page expands the full engine surface (sub-namespaces and
  root methods) and becomes the reference's front door, linking out to each
  domain page.
- **Group the index by domain, not by reflection kind.** Tag the
  declaration with `@group`/`@groupDescription` and order the groups in the
  TypeDoc config (`Start Here`, `Graphics`, `Math`, `Input`, `Physics`,
  `Audio`, `Values`, `Configuration`, `Events`, `Enums`, `Results`, `*`),
  with `indexFormat: "table"` for one-line summaries beside each link.
- **Hide the CommonJS authoring facilities** (`module`, `exports`,
  `require`, `__dirname`, `__filename`, `EfxRequire`, `EfxModule`,
  `EfxModuleCacheEntry`) from the public reference — they are module-scoped
  authoring plumbing, not members of `efx`.
- **Inline per-function option bags.** Mark `*Options` configuration
  interfaces `@hidden @inline` so they get no index entry and their fields
  render inline in the functions that accept them (LÖVE documents options
  as parameters). Reusable value records (`Material`, `SourceRect`,
  `PoseSample`, the shape descriptors, channels), event payloads, enums,
  results, and native-backed resource classes keep their own pages, grouped
  under the domain that owns them.
- **Add a curated landing page.** A short, hand-authored introduction
  (what EFX is, the smallest complete script, "start with `efx`") is merged
  as the root of both the committed Markdown and the published HTML
  reference.
- Regenerate the committed Markdown `docs/api/` and the HTML `/api`
  rendering; the drift guard (`docs:check`) stays green.
- Record the reference-organization rule in `docs/js-api.md`.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `js-api`: the **Normative API reference document** requirement changes to
  permit a curated hand-authored landing page and tag-driven organization
  of the generated reference, while keeping per-symbol detail generated
  from `gallery/src/api/efx.d.ts` and drift-checked.

## Impact

- `gallery/src/api/efx.d.ts` — TSDoc tags only; **no type or signature
  changes**.
- `gallery/typedoc.json`, `gallery/typedoc.markdown.json`,
  `gallery/typedoc.html.json` — grouping/ordering/landing options.
- A new landing-page source file consumed by TypeDoc (location chosen in
  design.md), merged into both renderings.
- `docs/api/**` — fully regenerated (committed reference).
- `gallery/scripts/check-docs.mjs` — unchanged unless the landing source
  lives outside the generated tree and needs to be copied in.
- `docs/js-api.md` — a reference-organization rule added to the guidelines.
- **No ADR.** This changes documentation presentation, not a cross-cutting
  architecture invariant; the durable rule lives in the `js-api` spec and
  `docs/js-api.md`.

## Non-goals

- No change to the JS API surface, signatures, semantics, or runtime
  behavior.
- No restructuring of `efx.d.ts` into namespaces/modules.
- No custom TypeDoc plugin or post-processor — tag-driven only.
- No hand-written per-symbol catalog; per-symbol detail stays generated.
- No new tutorials or guide pages beyond the single landing page.
