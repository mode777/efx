# Tasks

## 1. Runtime evaluation helper

- [x] 1.1 Add a runtime helper that evaluates one REPL line in the persistent
  global context and reports the result/exception without setting the fatal
  error flag. Verify a headless unit test runs two lines where the first
  throws and the second still evaluates, with the result printed.

## 2. Run mode and input loop

- [x] 2.1 Parse `--repl [<root>]` and open the normal window/frame loop. Verify
  `player --repl` (no root) starts and accepts piped input under a display.
- [x] 2.2 Poll stdin non-blocking in the frame callback, buffer partial lines,
  and evaluate complete lines; print a prompt only on a TTY. Verify a piped
  multi-line run evaluates each line and the captured stdout contains no
  prompts.

## 3. Commands, root, and exit contract

- [x] 3.1 Handle `.help` and `.exit` before evaluation without adding `efx`
  API. Verify `.help` prints the command list and `.exit` ends with code 0.
- [x] 3.2 When a root is given, set it and evaluate its `main.js` if present
  before input; make `load*` usable from the console. Verify a piped session
  loads a fixture resource from the root.
- [x] 3.3 Implement the exit contract: `efx.quit(n)` exits n, EOF exits 0, a
  bad `--repl` root prints a diagnostic and exits non-zero, and Emscripten
  reports the mode unavailable. Verify each with a test.

## 4. Docs and ADR

- [x] 4.1 Amend ADR 0007 to record the third run mode (console) and its exit
  contract; update the ADR index if needed.
- [x] 4.2 Add the REPL note to `docs/js-api.md` and `gallery/src/api/efx.d.ts`
  (no new API — the console drives the existing namespace).
- [x] 4.3 Update the AGENTS.md roadmap/current-state rows for the F6d slice.

## 5. Verification

- [ ] 5.1 Register a ctest case that pipes a session exercising evaluation,
  state persistence, error recovery, `.help`/`.exit`, and `efx.quit` codes;
  gate it on a display like the golden suite. Verify it passes on Linux under
  `xvfb-run`.
- [ ] 5.2 Run the Linux pipeline first, then Windows, then macOS; verify via
  `python3 tools/verify_remote.py all <branch>` before dispatching
  `gh workflow run ci.yml --ref <branch>`.
- [ ] 5.3 Confirm the four-target gate is green before archiving.
