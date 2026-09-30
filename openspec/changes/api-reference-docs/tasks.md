# Tasks

## 1. Tooling and configuration

- [x] 1.1 Add pinned `typedoc` and `typedoc-plugin-markdown` devDependencies to `gallery/package.json` and install; verify `npm --prefix gallery install` updates the lockfile and `npx --prefix gallery typedoc --version` reports the pinned version
- [x] 1.2 Add `gallery/typedoc.json` (shared base: `entryPoints` = `src/api/efx.d.ts`, `exclude` = `src/api/efx.type-test.ts`, `name` = "EmotionFX API", `disableSources`), `gallery/typedoc.markdown.json` (markdown plugin, `out` = `../docs/api`), and `gallery/typedoc.html.json` (`out` = `dist/api`, `customCss`); verify each config loads with `npx --prefix gallery typedoc --options <config> --emit none`
- [x] 1.3 Add `gallery/typedoc-ps2.css` overriding the default theme's palette variables (dark background, PS2 accent); verify a local HTML generation renders dark with the accent applied

## 2. Generate and commit the Markdown reference

- [x] 2.1 Add the `docs:markdown` script (`typedoc --options typedoc.markdown.json`), run it, and commit `docs/api/`; verify `docs/api/README.md` and `docs/api/interfaces/Efx.md` exist and that a symbol with an `@example` (e.g. `EfxKeyboard.md`) contains a rendered fenced code block
- [x] 2.2 Add the `docs:check` script that regenerates the Markdown to a temporary directory and diffs it against `docs/api/`, exiting non-zero on any difference; verify it passes on the committed tree and fails after a deliberate declaration edit that is not regenerated
- [x] 2.3 Remove the two stale `docs/js-api.md` cross-references in `gallery/src/api/efx.d.ts` (file header and `EfxKey`) and regenerate; verify the resulting `docs/api/` diff is limited to the affected symbol files

## 3. Repurpose `docs/js-api.md` as design guidelines

- [x] 3.1 Rewrite `docs/js-api.md`: remove the per-function catalog sections, retain and expand the design rules (namespace, two-layer structure, naming and option-bag conventions, units/colors, error model, resource and memory model, fixed limits, lifecycle, module model, gamepad model), add an "adding to the API" process, and add a pointer to the generated reference; verify the document contains no per-function signature catalog and still contains the resource-classification and fixed-limits tables
- [x] 3.2 Update `AGENTS.md`: change the script-facing API rule to require regenerating `docs/api/` from the declaration, and update the Documentation section (`docs/js-api.md` = guidelines; `docs/api/` = generated reference published at `/api`); verify the rule and the Documentation section read consistently with `docs/js-api.md`
- [x] 3.3 Update the `proposal`/`tasks` rules in `openspec/config.yaml` that name `docs/js-api.md` to also require `docs/api/` regeneration; verify `npx openspec validate --strict` still passes
- [x] 3.4 Add an explicit, unmissable `AGENTS.md` note that `docs/api/` is auto-generated and must not be manually edited; verify the note appears in the Documentation section and the docs/api bullet says "do not edit by hand"

## 4. Pipeline and site integration

- [x] 4.1 Add the `docs:html` script (`typedoc --options typedoc.html.json`); verify that, after `npm --prefix gallery run build`, it emits `gallery/dist/api/index.html`
- [x] 4.2 Update `.github/workflows/pages.yml` to run `docs:check` and `docs:html` after the gallery build and to assert `gallery/dist/api/index.html` in the output check; verify the workflow file parses and the new steps run after `npm --prefix gallery run build`
- [x] 4.3 Add an "API Reference" link in the gallery shell pointing to `./api/`; verify `npm --prefix gallery run check` passes and the link is present in the built page

## 5. ADR and verification

- [x] 5.1 Write `docs/decisions/0042-api-reference-generated-from-type-doc.md` per `docs/decisions/TEMPLATE.md` and add it to the `docs/decisions/README.md` index; verify the index links the new ADR
- [x] 5.2 Verification: run `npm --prefix gallery run check` and `npm --prefix gallery run docs:check`, then `npm --prefix gallery run build` followed by `npm --prefix gallery run docs:html`, and confirm both the gallery build and `dist/api/index.html` succeed; then dispatch the Pages workflow on a branch and confirm the site serves the reference at `/api`. The four-target gate is unaffected (no engine or runtime code changes), so ctest/goldens are not run for this change
