# Tasks

## 1. Mesh data model

- [x] 1.1 Extend `efx_surface_src`/`efx_surface` with `joints` (four per
  vertex) and `weights` (four per vertex) CPU arrays. Verify a headless build
  compiles and existing mesh tests are unaffected.
- [x] 1.2 Accept `joints`/`weights` in `createMeshData` with pair and
  vertex-count validation (`RangeError` on mismatch, `TypeError` on wrong
  element type). Verify unit tests cover valid skinned data, an unpaired
  attribute, and a count mismatch.

## 2. Skin and skeleton import

- [x] 2.1 Parse `JOINTS_0`/`WEIGHTS_0` into surface attributes, normalizing
  joint component types (u8/u16) and validating counts. Verify unit tests for
  each component type and a malformed-count error.
- [x] 2.2 Resolve the selected mesh's node → `skin`, bundling the joint
  hierarchy and inverse bind matrices (identity-filled when absent). Verify a
  unit test asserts the joint list, parent relationships, and an inverse bind
  matrix value; a static mesh carries no rig.

## 3. Animation clips

- [x] 3.1 Parse `animations[]` channels and samplers (target node/path, times,
  values, interpolation) into the clip payload; assign names (index-based when
  unnamed). Verify a unit test asserts channel count, times, and values for a
  LINEAR clip.
- [x] 3.2 Implement STEP import and the CUBICSPLINE→LINEAR approximation
  (keep the middle value per keyframe, drop tangents). Verify unit tests over
  known STEP and cubic sequences, asserting no import failure.

## 4. Payload carry and lifetime

- [x] 4.1 Deep-copy the rig payload from `MeshData` onto `Mesh` at
  `createMesh`; ensure destroying the `MeshData` afterwards leaves the `Mesh`
  rig intact. Verify a unit test imports, builds, destroys the MeshData, and
  still reads the rig through the Mesh.
- [x] 4.2 Confirm no new JS shape is exposed — only joints/weights attributes
  are script-visible. Verify the API tests assert no clip/joint query property
  exists on MeshData/Mesh.

## 5. Bindings

- [x] 5.1 Expose the joints/weights surface fields through the quickjs binding
  with the same validation as other attributes. Verify a ctest script test
  builds a skinned MeshData and asserts success plus the mismatch errors.
- [ ] 5.2 Mirror the fields through the web bridge (`entry.js` attribute
  marshalling). Verify the web compare harness runs the skinned script with
  output matching desktop.

## 6. Docs and ADR

- [x] 6.1 Write the rig-payload ADR (`docs/decisions/NNNN-*`, per
  `TEMPLATE.md`) recording the attribute model, payload carry, interpolation
  policy, and opaqueness; add it to `docs/decisions/README.md`.
- [x] 6.2 Update `docs/js-api.md` and `gallery/src/api/efx.d.ts` for the
  joints/weights surface attributes and the implicit rig payload.
- [ ] 6.3 Update the AGENTS.md roadmap/current-state rows for the F6c slice.

## 7. Verification

- [x] 7.1 Commit a skinned glTF fixture (with a skeleton and at least one
  LINEAR clip, plus one CUBICSPLINE clip for the approximation path).
- [ ] 7.2 Add a rest-pose golden of the skinned asset; capture it server-side
  (llvmpipe, per `docs/verification-server.md`) and commit it. Verify the
  native and web golden suites pass.
- [ ] 7.3 Run the Linux pipeline first (ctest smoke + goldens), then Windows,
  then macOS; verify via `python3 tools/verify_remote.py all <branch>` before
  dispatching `gh workflow run ci.yml --ref <branch>`.
- [ ] 7.4 Confirm the four-target gate is green before archiving.
