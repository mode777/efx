## 1. Root CMake (R17)

- [x] 1.1 On `main`, capture the baseline:
  - sorted `compile_commands.json` from a Ninja configure on the Linux server
    (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DEFX_BUILD_GOLDEN_TESTS=ON`);
  - `ctest -N -V` from the desktop and Emscripten builds.

  Attach both to the PR. (Stored under `baseline/` in this change folder,
  following the `refactor-volume-tests` precedent: desktop 298 tests,
  Emscripten 159.)
- [x] 1.2 Add `EFX_RENDER_CORE_SOURCES`, `EFX_PHYSICS_SOURCES` and
  `EFX_API_SOURCES`. Build `efx_core` from one common list, with the desktop
  variant appending the API/runtime sources. Verify: the sorted
  `compile_commands.json` is identical.
- [x] 1.3 Add `efx_add_test_exe(...)` (D1) and convert the 8 test executables,
  keeping every per-target difference as an explicit argument. Verify: the
  sorted
  `compile_commands.json` is identical, and V1 + V2 pass. (E5 identical on the
  server; V1 green locally, 191/191; the V2 golden build runs as part of V4 on
  the server — no local display.)

## 2. Test registration (R18)

- [x] 2.1 Extract `efx_add_run_test()` from
  `add_player_test`/`add_web_test`, which become thin wrappers. Verify:
  `ctest -N -V` is identical on desktop. (Compared name-keyed: all 298 names
  and per-test commands byte-identical; registration order of the portable
  rows moved below the explicit blocks — inventory and behavior unchanged.
  Full native ctest incl. goldens green on the server.)
- [x] 2.2 Add `efx_portable_case(...)` and convert only the rows whose
  arguments are identical for both runtimes (D2). Keep every other case
  explicit. Verify: `ctest -N -V` is identical on desktop (V2) and on
  Emscripten (V4). (23 portable rows converted; name-keyed `ctest -N -V`
  identical on both runtimes; Emscripten ctest 159/159 green on the server.
  One found-and-fixed defect: DESKTOP_ONLY/NO_EM_GROWTH were declared as
  one-value keywords, so the Emscripten skip silently no-opped — caught by
  the server web build, not by the desktop-only proofs.)

## 3. Web runner host page (R19)

- [x] 3.1 Add `hostPage({ title, script, verbose })` to
  `tools/lib/web-host.mjs`, and use it in `run_web_goldens.mjs`
  (`verbose: true`) and `run_web_harness.mjs`. Verify on V4: web goldens and
  harness scenarios pass, with unchanged stdout and exit codes. (Web goldens
  all pass; 9/9 harness scenarios; cross-runtime compare all match — same
  server session.)

## 4. Comment hygiene (R20)

- [x] 4.1 Apply the D4 rules module by module (`api`, `web`, `render`,
  `platform`, `resource`, `physics`, `input`, `audio`, `runtime`/`player`,
  `prelude`), one commit per module. Verify per commit: the
  `cc -fpreprocessed -dD -E` output of each touched C/C++ file is identical
  (E7). (One commit per module group; E7 green for every touched file; ADR
  0019/0020/0021/0024/0025/0026/0027/0028/0032/0033/0035/0036/0040/0041/0042/0043
  now carry the pointers the archived design docs used to.)
- [x] 4.2 Regenerate `src/prelude/prelude.h` if `prelude.js` comments changed.
  Verify: `python tools/gen_prelude.py --check` passes (V3), and a grep for
  `design D[0-9]` and `(P[0-9]+)` in `src/` returns only intentional
  survivors, each listed in the PR. (Regenerated; check passes; the grep
  returns zero survivors.)

## 5. Re-home misplaced desktop functions (R21, optional)

- [x] 5.1 Move `drawQuad`/`read_quad_opts`/`setBlendMode` to `api_2d.c` and
  `poseMesh`/`read_pose_sample`/`setCamera3D` to `api_3d.c` (D6). Verify:
  `git diff -M --color-moved=dimmed-zebra` shows only moves and declarations;
  V1 + V2 pass. Drop the task if any non-move edit is required. (Found them
  in `api_target_post.c`/`api_particles.c` post-`refactor-volume-core`;
  python line-range move verified byte-identical — 451 insertions / 450
  deletions, the +1 per file being the paste separator. V1 191/191.)

## 6. AGENTS.md slimming (signed off 2026-10-01)

- [x] 6.1 Rewrite "Current state" as summary + per-milestone links + codebase
  map (D5). Move the operational-rule block (current L382–L441) unchanged.
  Verify: a diff of that block is empty, and every relative link in
  `AGENTS.md` resolves. (Block diffed empty after the move; 569 → 268 lines;
  all 29 relative paths resolve. One pre-existing drift fixed by one token:
  the Roadmap table's F13 row named `src/input/efx_gamepad.c`, renamed to
  `gamepad.c` by `refactor-volume-core` — a path correction, not a table
  restructure.)
- [x] 6.2 Correct "Not yet decided": the glTF import profile was settled in F6
  (ADR 0032). Remove the matching "remains open" sentence. Verify: no
  remaining statement in `AGENTS.md` contradicts `docs/decisions/README.md`.
  (Zero "remains open"/unresolved statements left.)
- [x] 6.3 Reviewer checklist: sample at least 10 removed facts (e.g. the
  F12 ADR, the F5b effect list, the F14 voice count, a gate run id) and record
  where each one is reachable (roadmap spec, ADR, archived change, or
  `docs/api/`). (15 facts sampled; `reviewer-checklist.md` in this change
  folder; every cited path verified to exist.)

## 7. Checkpoint 3 verification

- [x] 7.1 Run the full local suite: V2 and V3. Confirm
  `s_error_catalog.expected.txt` is unchanged and the `ctest -N` inventory
  equals the post-`refactor-volume-core` inventory. Record the volume Δ (E4).
  (V1 fresh build 191/191 with zero warnings; V3 prelude + docs green; E6
  zero unused exports; error catalog byte-unchanged; ctest inventory
  name-keyed identical 298/159 against the baseline captured in 1.1 — V2's
  golden build runs on the server as part of V4, this container has no
  display. Volume Δ (E4): 39 439 → 39 349 non-blank lines, −90 for
  code+tools; plus AGENTS.md 569 → 268 lines, −301, outside the E4 paths.)
- [x] 7.2 Push and run V4
  (`python3 tools/verify_remote.py all refactor-volume-build`). Verify: green.
  (All three suites passed: native 298/298 incl. all golden scenes under
  Xvfb+llvmpipe, web goldens, gallery smoke. The Emscripten ctest (159/159),
  web harness and cross-runtime compare were additionally run on the server
  during R18/R19.)
- [x] 7.3 Dispatch V5 (`gh workflow run ci.yml --ref refactor-volume-build`)
  in the order Linux → Windows → macOS. Verify: green; record the run id.
  (Run 36864472674: Linux, Windows, macOS, Emscripten, web goldens, curated
  samples — all success.)

## 8. Docs and close-out

- [ ] 8.1 Update `docs/refactoring.md`: mark R17–R21 done (or R21 dropped),
  note the `AGENTS.md` slimming done (the former P21), and record Checkpoint 3
  and the measured Δ for Part 1.
- [ ] 8.2 Merge to `main` and push (per `AGENTS.md`), then archive the change
  (`skip_specs`).
