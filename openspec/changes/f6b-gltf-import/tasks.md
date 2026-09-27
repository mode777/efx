# Tasks

## 1. Dependency and parse path

- [x] 1.1 Vendor `cgltf` at a pinned revision (`vendor/cgltf/`) and update
  `vendor/README.md` with license and pin. Verify the tree builds on the
  headless configuration under `-Wall -Wextra -Werror` (cgltf warnings
  suppressed only within its isolated TU if needed).
- [x] 1.2 Add a provider-fed `cgltf_options.file.read` callback that resolves
  URIs relative to the glTF path through the F6a provider. Verify a headless
  unit test loads a `.glb` and a `.gltf` with an external `.bin` from both a
  directory and a zip root.
- [x] 1.3 Implement the parse/validation layer: report malformed assets,
  unsupported `extensionsRequired`, and mesh-selection errors as distinct
  failures. Verify unit tests assert each error case.

## 2. Geometry and surface mapping

- [x] 2.1 Map the selected glTF mesh's primitives to `MeshData` surfaces
  (positions, normals, uvs, colors, indices), enforcing the 1..16 surface cap
  and mesh selection by index/name. Verify a unit test asserts surface count,
  attribute lengths, and unknown-mesh errors.
- [x] 2.2 Normalize accessor component types (u8/u16/u32/float, normalized,
  strided, sparse) into the engine's float attribute layout. Verify unit tests
  cover each component type and a sparse accessor against known values.
- [x] 2.3 Confirm imported geometry is untransformed. Verify a fixture whose
  mesh node has a non-identity transform imports as authored.

## 3. Materials

- [x] 3.1 Implement the PBR→Phong conversion (base/emissive/metallic/
  roughness/alpha MASK) per design D4. Verify unit tests assert each channel
  mapping and the default material for a primitive without a material.
- [x] 3.2 Bind converted materials per surface on the `MeshData`. Verify a
  unit test reads the bindings back through the render API and a golden
  renders a multi-material asset.

## 4. Textures and samplers

- [x] 4.1 Extend `createTexture`/the render sink with `{ wrap, filter }` and
  cache platform samplers per combination, keeping defaults repeat/linear.
  Verify existing goldens are byte-identical and a new unit test asserts the
  resolved sampler for each option.
- [x] 4.2 Import glTF images (external, data-URI, buffer view) with per-image
  decode and per-(image, sampler) texture dedup, mapping glTF samplers and
  falling back to defaults. Verify a unit test asserts dedup identity and
  sampler mapping; a corrupt image errors.
- [x] 4.3 Retain imported textures on the `MeshData` until `destroy()`/
  `createMesh`. Verify a unit test imports, drops all texture references,
  builds the mesh, and renders with the maps intact.

## 5. Bindings

- [x] 5.1 Expose `loadMeshData(path, opts?)` through the quickjs binding with
  `{ mesh }` validation and `Error`/`TypeError` semantics. Verify a ctest
  script test imports a fixture and asserts `surfaceCount`.
- [x] 5.2 Mirror `loadMeshData` and the `createTexture` sampler options through
  the web bridge; update `entry.js`/prelude as needed. Verify the web compare
  harness runs the import script with output matching desktop.

## 6. Docs and ADR

- [x] 6.1 Write the glTF-profile ADR (`docs/decisions/NNNN-*`, per
  `TEMPLATE.md`) pinning container, references, material mapping, samplers,
  and extension policy; add it to `docs/decisions/README.md`.
- [x] 6.2 Update `docs/js-api.md` and `gallery/src/api/efx.d.ts` for
  `loadMeshData` and the `createTexture` sampler options, and remove the
  provisional `loadMesh` entry in the same change.
- [x] 6.3 Update the AGENTS.md roadmap/current-state rows for the F6b slice.

## 7. Verification

- [ ] 7.1 Commit fixtures: a `.glb`, a `.gltf` package with external `.bin`/
  `.png`, and a multi-material + alpha-MASK asset.
- [ ] 7.2 Add a golden that imports and draws a committed asset; capture the
  golden server-side (llvmpipe, per `docs/verification-server.md`) and commit
  it. Verify the native and web golden suites pass.
- [ ] 7.3 Run the Linux pipeline first (ctest smoke + goldens), then Windows,
  then macOS; verify via `python3 tools/verify_remote.py all <branch>` before
  dispatching `gh workflow run ci.yml --ref <branch>`.
- [ ] 7.4 Confirm the four-target gate is green before archiving.
