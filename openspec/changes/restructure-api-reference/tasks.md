# Tasks

## 1. Landing source and generator config

- [x] 1.1 Author `gallery/src/api/README.md` as a short LÖVE-style introduction: what EFX is, the single `efx` global and its sub-namespaces as a link table, the smallest complete script (Hello Cube), and a pointer to `docs/js-api.md`. Verify it renders as plain Markdown and contains no per-symbol catalog.
- [x] 1.2 Set `readme` in `gallery/typedoc.json` to the landing source (replacing `"none"`) and add `"mergeReadme": true` plus `"indexFormat": "table"` to `gallery/typedoc.markdown.json`; add `groupOrder` to the shared config. Verify `npm --prefix gallery run docs:markdown` succeeds and `docs/api/README.md` contains the landing content followed by the generated index.

## 2. Tag taxonomy on the declaration

- [x] 2.1 Add `@group` to every top-level declaration in `gallery/src/api/efx.d.ts` using the D2 taxonomy (`Start Here`, `Graphics`, `Math`, `Input`, `Physics`, `Audio`, `Values`, `Configuration`, `Events`, `Enums`, `Results`). Verify the regenerated index shows domain headings in `groupOrder` and no `## Interfaces` / `## Type Aliases` / `## Variables` kind headings.
- [x] 2.2 Add `@groupDescription` lines for the groups where a one-line gloss helps. Verify the gloss appears under the group heading in `docs/api/README.md`.

## 3. Reference surface and page inventory

- [x] 3.1 Move the `Efx` interface's summary and Hello Cube `@example` onto the `efx` variable's TSDoc, then tag `Efx` `@hidden @inline` and tag `efx` `@group Start Here`. Verify `docs/api/interfaces/Efx.md` is gone, `docs/api/variables/efx.md` expands the surface with links to `EfxGraphics`/`EfxMath`/…, and `efx` leads the index.
- [x] 3.2 Tag the CommonJS authoring facilities (`EfxRequire`, `EfxModule`, `EfxModuleCacheEntry`, and the `require`/`module`/`exports`/`__filename`/`__dirname` variables) `@hidden`. Verify none of them appear in `docs/api/README.md` and their pages are gone.
- [x] 3.3 Tag each single-use `*Options` configuration interface `@hidden @inline`, and confirm reused records (`Material`, `SourceRect`, `PoseSample`, shape descriptors, `MeshSurfaceData`, channels, `FontOutline`, `FontShadow`) remain as grouped pages. Verify option fields render inline in the consuming operations and no `*Options` entry remains in the index.

## 4. Regenerate and commit the reference

- [x] 4.1 Run `npm --prefix gallery run docs:markdown` and commit the regenerated `docs/api/` (deleted `Efx` page, hidden authoring pages, inlined options, table index, merged landing). Verify `git status` shows only `docs/api/` and the source/config files for this change.
- [x] 4.2 Run `npm --prefix gallery run docs:html` and inspect `dist/api` to confirm the HTML home page shows the landing and the sidebar groups by domain. Verify the palette and gallery link-back are unchanged.

## 5. Guidelines and verification

- [x] 5.1 Update `docs/js-api.md`: add the reference-organization rule (domain-first groups, `@hidden @inline` for new single-use option bags, authoring facilities excluded, curated landing page), and note that new symbols reference `efx`'s members rather than `Efx`. Verify the "Adding to the API" steps mention tagging new option bags.
- [x] 5.2 Run `npm --prefix gallery run docs:check` and confirm it passes against the committed `docs/api/`.
- [x] 5.3 Run `npx openspec validate restructure-api-reference --type change --strict` and confirm it passes.
