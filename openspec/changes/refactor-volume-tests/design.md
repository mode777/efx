## Context

See `proposal.md` (Why) and `docs/refactoring.md` §2.5–§2.6, R0–R7. The current
facts that shape the approach:

- **Seven unit-test executables** (`math`, `render`, `input`, `audio`,
  `resource`, `text`, `api`). Each defines its own `fail()` (five also define
  `feq()`) and dispatches with a `strcmp` chain in `main`.
  `efx_physics_tests` instead uses a `CASE(fn)` table in `tests/physics/main.c`
  and, given no argument, runs every case. Its `tests/physics/test_support.h`
  holds physics-specific helpers (`CHECK`, `nearf`, `v3_near`, world builders).
- **Case registration.** `tests/CMakeLists.txt` registers cases through
  `add_unit_test(TARGET CASE)` in hand-written `foreach(CASE …)` lists
  (L176–L248). The same suites build for Emscripten, where ctest runs them
  through `node`. `resource` and `text` are desktop-only.
- **Baseline at `cdf1c67`:** 40 560 non-blank hand-maintained lines (the
  `docs/refactoring.md` §1 command). The four orphaned cases pass when run
  directly on Windows/MSVC Release.
- **Over-exported functions.** About 35 header-declared `efx_*` functions are
  referenced only inside their defining file. The candidate list comes from a
  scan of `src/**/*.h` declarations against references in `src/` and `tests/`.
- **Duplicate web helpers.** `src/web/js/core.js` defines `__efxAllocCStr`
  twice: top level, using `Module`, and inside `__efxEnsureApi`, using
  `bridge` (which is `Module`). `efx_bridge_mem_free(p)` is exactly `free(p)`,
  and `_free` is already in `EXPORTED_FUNCTIONS`.
- **Resource exposure is untouched.** No script-visible resource class, GC
  finalizer or `destroy()` path changes (ADR 0011/0012 hold as-is).

### Recorded baseline (R0, `main` @ `cdf1c67`)

Recorded on Windows/MSVC 2022 (D3D11) and a local emsdk 5.0.5 build (the SSH
server was unavailable, so the Emscripten inventory comes from a local
`emcmake cmake -B build-em -G Ninja` build instead of V4). The sorted lists are
in `baseline/`:

| Evidence | File | Count |
|----------|------|------:|
| E4 volume (§1 PowerShell command) | — | 40 561 |
| E2 headless `ctest -N` (V1, `build-h`) | `ctest-headless.txt` | 186 |
| E2 desktop `ctest -N` (V2, `build`) | `ctest-desktop.txt` | 293 |
| E2 Emscripten `ctest -N` (`build-em`) | `ctest-emscripten.txt` | 156 |
| E3 desktop `efx_core.lib` `efx_*` globals (`llvm-nm -g --defined-only`) | `efx_core-symbols.txt` | 403 |
| E3 web `libefx_core.a` `efx_*` globals | `efx_core-web-symbols.txt` | 305 |

The bash variant of the §1 command reported 40 560; the PowerShell one counts
one more line (whitespace-only line treatment differs). Every later Δ uses the
PowerShell command. All three suites were green at baseline.

## Goals / Non-Goals

**Goals:**

- Make "a case exists" and "a case runs in ctest" the same fact on every
  target.
- Delete test and production boilerplate without changing any assertion,
  message, exit code, or the script-facing API.

**Non-Goals:**

- Adopting a third-party test framework.
- Merging the physics-specific helpers into the generic header.
- Changing which suites are desktop-only.

## Decisions

### D1 — A generic `tests/test_support.h`; the physics header builds on it

The new header provides `fail(const char *)`, `feq(float, float)` (the
existing 0.001 tolerance), the `EFX_CASE(fn)` table-entry macro, and
`efx_test_main(cases, n, argc, argv)`. `tests/physics/test_support.h` includes
it and keeps its own `CHECK`/`nearf`/`v3_near`/world helpers.

- **Why:** one runner and one assertion vocabulary across suites, while
  physics keeps helpers that only make sense there.
- **Rejected — vendor a framework (Unity, greatest):** it adds a pinned
  vendored dependency (ADR 0006) to replace about 30 lines.
- **Rejected — move everything into the physics header:** that would make
  every suite include `physics/physics.h`.

### D2 — CMake reads `EFX_CASE(name)` tokens from each suite source

`efx_register_unit_cases(target source)`:

- reads the source with `file(STRINGS … REGEX "EFX_CASE\\(")`;
- extracts names with `EFX_CASE\(([A-Za-z0-9_]+)\)`;
- adds the source to `CMAKE_CONFIGURE_DEPENDS`, so a new case reconfigures;
- calls the existing `add_unit_test` per name.

Only suite sources are scanned, never the header that defines the macro.

- **Why:** the C table is the only list a developer edits, so ctest cannot
  miss a case again.
- **Rejected — an X-macro `.def` file per suite:** it also removes drift, but
  adds seven files to save nothing beyond this.
- **Rejected — a `--list` query of the built binary:** Emscripten test
  binaries cannot run at configure time.
- **Rejected — keep the lists and add a checker script:** two sources remain,
  plus a new tool to maintain.

### D3 — One runner behavior: a case argument runs that case; no argument runs all

All suites adopt the physics behavior. An unknown case still prints
`unknown case: <name>` and exits 2.

- **Why:** ctest always passes a case name, so the no-argument path is a
  developer convenience. Unifying it removes a per-suite special case.
- **Rejected — keep "usage + exit 2" for the six non-physics suites:** that
  needs a flag in the shared runner for no gain.

### D4 — `REQUIRE` stays local to the JS-driven suites; `T_HELPER` is a compile-time literal

- `api_tests.c` defines
  `#define REQUIRE(c, msg) do { if (!(c)) { end_js(); return fail(msg); } } while (0)`.
  `resource_tests.c` gets the equivalent with its own clean-up call.
- The JS helper `function t(fn, kind)` becomes `#define T_HELPER "…"`, which
  snippets concatenate as adjacent string literals.
- **Why:** the clean-up step differs per suite, so a generic `REQUIRE` would
  need a callback. A literal keeps snippet line numbers unchanged.
- **Rejected — prepend the helper inside `run_js`:** every reported JS line
  number would shift, and the helper would leak into snippets that don't use
  it.
- **Rejected — `goto cleanup` per test:** it saves fewer lines and reads
  worse.

### D5 — Fix the never-run cases inside this change

R1 registers the four cases first, before any other edit, so they guard
everything after them. They already pass on Windows/MSVC.

If one fails on Linux, macOS or Emscripten during V4/V5:

- fix it in this change, as its own commit;
- record the root cause in this design under "Findings during apply".

ADR 0045 (tunneling) and the existing `js-api` behavior (`setClearColor`)
already require what these cases assert, so a fix restores specified behavior
and needs no spec delta.

### D6 — Remove the unused physics math; amend ADR 0040's wording

Delete `efx_quat`, `efx_mat3`, `efx_quat_identity`, `efx_mat3_identity`,
`efx_mat3_mul_vec3`, `efx_mat3_transpose`, `efx_mat3_mul`, `efx_v3_mul`,
`efx_v3_min_component` and `efx_aabb_contains`. Change ADR 0040's
"`vec3`/`quat`/`mat3` math" to "its own vector math".

- **Why:** physics is linear-only (ADR 0040). The point of that ADR is "no
  GLM dependency", which still holds.
- **Rejected — keep them for future rotation support:** YAGNI. Rotation would
  need its own milestone and spec anyway.
- **Rejected — a new ADR:** no new decision is made.

### D7 — Internal linkage, one commit per module, with an exclusion list

Each candidate becomes `static` and loses its header declaration.

Excluded:

- symbols named by a spec or ADR (`efx_input_inject_*`,
  `efx_input_gamepad_inject_*`, `efx_input_gamepad_load_mappings`);
- symbols referenced by `src/web/**`, `tools/**` or `tests/**`;
- anything a later change will export to the web build (R14 targets are
  already used by `bridge_*.c`, so they are not candidates).

Under `-Werror`/`/WX`, a function that becomes an unused `static` breaks the
build. It is then deleted.

- **Why:** a smaller public header surface, and the compiler flags truly dead
  code.
- **Rejected — one bulk commit:** a surprise reference would be harder to
  bisect.

### D8 — Web duplicates: keep the top-level helpers

- Keep the top-level `__efxAllocCStr`.
- Replace `__physNumber`/`__efxAudioNum` with one `__efxFiniteNumber(v, what)`
  in `core.js`. It keeps the identical message: TypeError
  `"<what> must be a finite number"`.
- Call `bridge['_free']` instead of `bridge['_efx_bridge_mem_free']`, and
  delete the export.
- **Why:** each pair is byte-equivalent, so only call sites change.
- **Rejected — keep `efx_bridge_mem_free` for symmetry with
  `efx_bridge_mem_alloc`:** the allocation wrapper has a size check; the free
  wrapper adds nothing.

## Risks / Trade-offs

- **[Risk] The CMake regex misses or invents a case name.** → E2: the
  `ctest -N` names must equal the R0 inventory plus the four R1 names on the
  headless, desktop and Emscripten builds. Any difference fails the pass.
- **[Risk] A newly registered case fails on a non-Windows target.** → D5: fix
  it in this change. Checkpoint 1 runs V5 on all four targets before merge.
- **[Risk] `static` exposes an unused function and breaks the build.** →
  Intended (D7). Delete the function in the same commit.
- **[Risk] `REQUIRE` changes failure-path clean-up.** → It expands to exactly
  the replaced block. Flip one assertion locally once to confirm the message
  and the clean-up.
- **[Trade-off] Scanning C source from CMake couples the table format to a
  regex.** → The format is one macro token, and drift becomes a visible E2
  failure.

## Migration Plan

1. Branch `refactor-volume-tests` from `main`. One commit per pass (R0 … R7)
   in the order of `tasks.md`.
2. After each pass, run V1 (headless), and V2 where noted.
3. After R7, run V4 (`python3 tools/verify_remote.py all refactor-volume-tests`),
   then V5 in the order Linux → Windows → macOS.
4. When green, merge to `main` and push (per `AGENTS.md`), then archive with
   `skip_specs`.
5. Rollback: every pass is an independent commit, so revert in reverse order.
   R1 (registration) is kept even if later passes are reverted.

## Findings during apply

- **R1 (no V4 server).** The SSH server was unavailable, so V4 was replaced
  by local runs. The four registered cases pass on Windows/MSVC (V1 + V2) and
  the three physics cases pass on a local Emscripten 5.0.5 build under Node
  (`clear_color_js` is desktop-only). Linux/GCC and macOS/Clang coverage of
  the four cases comes from the V5 gate. Inventories: headless 186 → 190,
  desktop 293 → 297, Emscripten 156 → 159 — exactly the R1 names.
- **R2 / D3 — run-all exposed two fresh-process assumptions.** With no
  argument, `efx_render_tests` failed `mesh_pending_upload` and
  `efx_api_tests` crashed on its second case:
  - `mesh_pending_upload` asserted the mock sink's process-wide create
    counters were still 0. It now zeroes them at case start (test-only).
  - `efx_api_init` (documented in `api.h` as per-context setup) skipped all
    class registration after the first runtime in the process
    (`static int registered`), so a second runtime's
    `JS_NewObjectClass` hit an unregistered class and crashed. The guard is
    deleted, so every runtime registers its classes (own commit). The player,
    REPL and dev harness create one runtime per process, so their behavior
    is unchanged; no spec or ADR describes the guard.
  Every suite now passes in run-all mode on Windows/MSVC.
- **R2 / D1 — `feq` tolerance.** `math_tests.c` compared with 0.0001, not
  0.001. The header's tolerance is `EFX_TEST_FEQ_EPS` (default 0.001f), and
  `math_tests.c` defines it as 0.0001f before including the header, so no
  assertion is weakened.
