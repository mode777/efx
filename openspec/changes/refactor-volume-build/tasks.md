## 1. Root CMake (R17)

- [ ] 1.1 On `main`, capture the baseline:
  - sorted `compile_commands.json` from a Ninja configure on the Linux server
    (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DEFX_BUILD_GOLDEN_TESTS=ON`);
  - `ctest -N -V` from the desktop and Emscripten builds.

  Attach both to the PR.
- [ ] 1.2 Add `EFX_RENDER_CORE_SOURCES`, `EFX_PHYSICS_SOURCES` and
  `EFX_API_SOURCES`. Build `efx_core` from one common list, with the desktop
  variant appending the API/runtime sources. Verify: the sorted
  `compile_commands.json` is identical.
- [ ] 1.3 Add `efx_add_test_exe(...)` (D1) and convert the 8 test executables,
  keeping every per-target difference as an explicit argument. Verify: the
  sorted `compile_commands.json` is identical, and V1 + V2 pass.

## 2. Test registration (R18)

- [ ] 2.1 Extract `efx_add_run_test()` from
  `add_player_test`/`add_web_test`, which become thin wrappers. Verify:
  `ctest -N -V` is identical on desktop.
- [ ] 2.2 Add `efx_portable_case(...)` and convert only the rows whose
  arguments are identical for both runtimes (D2). Keep every other case
  explicit. Verify: `ctest -N -V` is identical on desktop (V2) and on
  Emscripten (V4).

## 3. Web runner host page (R19)

- [ ] 3.1 Add `hostPage({ title, script, verbose })` to
  `tools/lib/web-host.mjs`, and use it in `run_web_goldens.mjs`
  (`verbose: true`) and `run_web_harness.mjs`. Verify on V4: web goldens and
  harness scenarios pass, with unchanged stdout and exit codes.

## 4. Comment hygiene (R20)

- [ ] 4.1 Apply the D4 rules module by module (`api`, `web`, `render`,
  `platform`, `resource`, `physics`, `input`, `audio`, `runtime`/`player`,
  `prelude`), one commit per module. Verify per commit: the
  `cc -fpreprocessed -dD -E` output of each touched C/C++ file is identical
  (E7).
- [ ] 4.2 Regenerate `src/prelude/prelude.h` if `prelude.js` comments changed.
  Verify: `python tools/gen_prelude.py --check` passes (V3), and a grep for
  `design D[0-9]` and `(P[0-9]+)` in `src/` returns only intentional
  survivors, each listed in the PR.

## 5. Re-home misplaced desktop functions (R21, optional)

- [ ] 5.1 Move `drawQuad`/`read_quad_opts`/`setBlendMode` to `api_2d.c` and
  `poseMesh`/`read_pose_sample`/`setCamera3D` to `api_3d.c` (D6). Verify:
  `git diff -M --color-moved=dimmed-zebra` shows only moves and declarations;
  V1 + V2 pass. Drop the task if any non-move edit is required.

## 6. AGENTS.md slimming (signed off 2026-10-01)

- [ ] 6.1 Rewrite "Current state" as summary + per-milestone links + codebase
  map (D5). Move the operational-rule block (current L382–L441) unchanged.
  Verify: a diff of that block is empty, and every relative link in
  `AGENTS.md` resolves.
- [ ] 6.2 Correct "Not yet decided": the glTF import profile was settled in F6
  (ADR 0032). Remove the matching "remains open" sentence. Verify: no
  remaining statement in `AGENTS.md` contradicts `docs/decisions/README.md`.
- [ ] 6.3 Reviewer checklist: sample at least 10 removed facts (e.g. the
  F12 ADR, the F5b effect list, the F14 voice count, a gate run id) and record
  where each one is reachable (roadmap spec, ADR, archived change, or
  `docs/api/`).

## 7. Checkpoint 3 verification

- [ ] 7.1 Run the full local suite: V2 and V3. Confirm
  `s_error_catalog.expected.txt` is unchanged and the `ctest -N` inventory
  equals the post-`refactor-volume-core` inventory. Record the volume Δ (E4).
- [ ] 7.2 Push and run V4
  (`python3 tools/verify_remote.py all refactor-volume-build`). Verify: green.
- [ ] 7.3 Dispatch V5 (`gh workflow run ci.yml --ref refactor-volume-build`)
  in the order Linux → Windows → macOS. Verify: green; record the run id.

## 8. Docs and close-out

- [ ] 8.1 Update `docs/refactoring.md`: mark R17–R21 done (or R21 dropped),
  note the `AGENTS.md` slimming done (the former P21), and record Checkpoint 3
  and the measured Δ for Part 1.
- [ ] 8.2 Merge to `main` and push (per `AGENTS.md`), then archive the change
  (`skip_specs`).
