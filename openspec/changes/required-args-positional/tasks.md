# Tasks

## 1. Declaration and type document

- [x] 1.1 Update `gallery/src/api/efx.d.ts` to the new signatures: `createImageData(width, height, pixels, opts?)`, `createRenderTarget(width, height)`, `setCamera3D(pos, target, fov, opts?)`, `createFont(fontData, size, opts?)`, `drawBillboard(texture, pos, opts?)`, `createParticleSystem(texture, max, lifetime, opts?)`, `createBody(shape, opts?)`, `createCharacter(radius, height, opts?)`, `raycast(origin, direction, maxDistance, opts?)`, `createMeshData(surfaces, materials?)`, and `drawQuad(texture, x, y, opts?)`; split the affected option interfaces (remove the extracted required fields), rename `DrawMeshCallOptions` to `DrawMeshOptions`, and drop `MeshDataBatch`/`MeshDataShorthand`/`CreateMeshDataOptions`. Verify with `npx tsc --noEmit` over the gallery type test.
- [x] 1.2 Update `gallery/src/api/efx.type-test.ts` to the new call forms and add `@ts-expect-error` cases proving a required input cannot be supplied only in a bag, the old call shapes are rejected, and `createMeshData`'s shorthand is gone. Verify `npx tsc --noEmit` passes with no unused `@ts-expect-error`.
- [x] 1.3 Update the TSDoc on every changed symbol (params, defaults, errors, examples) so the generated reference reads correctly. Verify no stale signature text remains (`rg` for the old forms in `efx.d.ts`).

## 2. Shared prelude (cold functions)

- [x] 2.1 Rewrite the nine cold prelude wrappers in `src/prelude/prelude.js` to read required values from positional arguments and validate only the remaining optional bag: `createImageData`, `createRenderTarget`, `setCamera3D`, `createFont`, `createParticleSystem`, `createBody`, `createCharacter`, `raycast`, `createMeshData` (positional surfaces + optional materials). Keep native marshalling unchanged and preserve every error class and message. Verify the cold-path unit tests pass.
- [x] 2.2 Regenerate `src/prelude/prelude.h` and confirm no drift. Verify `python3 tools/gen_prelude.py --check` passes.

## 3. Hot native validators (draw reorder)

- [x] 3.1 Reorder `drawQuad` to `(texture, x, y, opts?)` in `src/api/api_2d.c`, adjust its arity in `src/runtime/runtime.c`, and update the web validator in `src/web/js/particles.js`; update the ADR 0049 hot-path carve-out list if needed. Verify the desktop/web error messages stay identical via the `smoke_error_catalog` contract.
- [x] 3.2 Reorder `drawBillboard` to `(texture, pos, opts?)` in `src/api/api_particles.c`, adjust its arity in `src/runtime/runtime.c`, and update `src/web/js/particles.js`. Verify the cross-runtime error catalog remains byte-identical apart from re-pathed trigger lines.

## 4. In-repo callers

- [x] 4.1 Update the portable script suite (`tests/scripts/*`), golden-scene `main.js` files (`tests/goldens/*`), and web fixtures (`tests/fixtures/web/*`) to the new call shapes. Verify `ctest` on a headless build passes (golden tests excluded) and the error catalog expected output matches.
- [x] 4.2 Update curated gallery samples (`gallery/samples/curated/*/main.js`) and `examples/*` to the new call shapes. Verify `npm --prefix gallery run build` succeeds.

## 5. Guidelines, reference, and ADR

- [x] 5.1 Rewrite the `docs/js-api.md` §Conventions "Parameters" section to state the mandatory rule (required positional / all-optional trailing bag), the lone-optional allowance, the record-versus-bag boundary, and the ordering rule; remove the hot-path/config-volume heuristic and update any signature examples elsewhere in the document. Verify no stale signature remains (`rg` for the old forms in `docs/js-api.md`).
- [x] 5.2 Regenerate the committed reference `docs/api/` from the declaration. Verify `npm --prefix gallery run docs:check` passes.
- [x] 5.3 Write the ADR recording the convention (next free number under `docs/decisions/`, per `TEMPLATE.md`) and add it to the `docs/decisions/README.md` index. Verify the ADR is indexed and its Status is set.

## 6. Verification

- [x] 6.1 Validate the change artifacts. Verify `npx openspec validate required-args-positional --strict` passes.
- [x] 6.2 Build headless and run the unit/smoke suites. Verify `cmake -B build -DEFX_HEADLESS=ON && cmake --build build && ctest --test-dir build -E golden` passes.
- [x] 6.3 Verify on the SSH server first: commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, and confirm the two golden-bearing jobs are green with golden frames byte-identical. Fix and re-verify until green.
- [ ] 6.4 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and iterate Linux → Windows → macOS until green; confirm `docs:check` and `gen_prelude.py --check` pass in CI.
