# Tasks

## 1. Display-name sweep (`EmotionFX` → `EFX`)

- [x] 1.1 Update prose titles and brand mentions in `README.md`,
  `CONTRIBUTING.md`, `vision.md`, the `docs/js-api.md` title, `AGENTS.md`,
  and the `openspec/config.yaml` domain line. Verify:
  `git grep -n 'EmotionFX' -- README.md CONTRIBUTING.md vision.md docs/js-api.md AGENTS.md openspec/config.yaml`
  returns nothing.
- [x] 1.2 Update user-visible source strings and comments: the window title in
  `src/platform/platform.c`, the REPL banner in `src/player/repl.c`, the
  header comments in `shaders/{quad,mesh,post}.glsl`, and the comment in
  `src/prelude/prelude.js`. Verify the headless build compiles
  (`cmake -B build -DEFX_HEADLESS=ON && cmake --build build -j4`) and
  `git grep -n 'EmotionFX' -- src shaders` returns nothing.
- [x] 1.3 Update gallery UI text and metadata: `gallery/index.html`,
  `gallery/public/runner.html`, `gallery/src/lib/Frame.svelte`,
  `gallery/src/lib/SampleList.svelte` (brand label), `gallery/typedoc.json`
  (`name`), `gallery/package.json` (`description`), and the header comment in
  `gallery/src/api/efx.d.ts`. Verify `git grep -n 'EmotionFX' -- gallery`
  (excluding `gallery/node_modules`) returns nothing.
- [x] 1.4 Update the prose mentions in `docs/decisions/0040-physics-core.md`
  and `docs/decisions/0044-curated-sample-dirs.md`. Verify
  `git grep -n 'EmotionFX' -- docs/decisions` returns nothing.

## 2. Slug, metadata, and tooling sweep (`emotion-fx` → `efx`)

- [x] 2.1 Update `package.json` (`name`, `repository.url`, `bugs.url`,
  `homepage`) to the `efx` slug, then run `npm install` to refresh
  `package-lock.json`. Verify `git grep -n 'emotion-fx' -- package.json package-lock.json`
  returns nothing and `npx openspec status --change rebrand-to-efx` still runs.
- [x] 2.2 Update the clone URL in `CONTRIBUTING.md` and all gallery/API/release
  links in `README.md` to `github.com/mode777/efx` and
  `mode777.github.io/efx/`. Verify
  `git grep -n 'mode777/emotion-fx\|mode777.github.io/emotion-fx' -- README.md CONTRIBUTING.md`
  returns nothing.
- [x] 2.3 Update the artifact names in `.github/workflows/ci.yml` (three
  `emotion-fx-` archive names) and the default in
  `gallery/scripts/pack-samples.mjs` to `efx-<version>-…`; update the stale
  subpath example comment in `gallery/vite.config.ts`. Verify
  `git grep -n 'emotion-fx' -- .github gallery/scripts gallery/vite.config.ts`
  returns nothing.
- [x] 2.4 Update the remote checkout default in `tools/verify_remote.py`
  (`--dir` default `~/efx`) and the matching note in
  `docs/verification-server.md`. Verify
  `python3 tools/verify_remote.py --help` prints `~/efx` and
  `git grep -n 'emotion-fx' -- tools/verify_remote.py docs/verification-server.md`
  returns nothing.
- [x] 2.5 Change the CMake project name to `efx` in `CMakeLists.txt`. Verify a
  fresh configure succeeds (`cmake -B build -DEFX_HEADLESS=ON`).

## 3. Regenerate committed artifacts

- [x] 3.1 Regenerate `src/prelude/prelude.h` with
  `python3 tools/gen_prelude.py`. Verify `python3 tools/gen_prelude.py --check`
  passes.
- [x] 3.2 Regenerate the API reference with
  `npm --prefix gallery run docs:markdown`. Verify
  `npm --prefix gallery run docs:check` passes and
  `git grep -n 'EmotionFX' -- docs/api` returns nothing.

## 4. Golden re-baseline

- [x] 4.1 Change the drawn string in `tests/goldens/text_basic/main.js` from
  `'EmotionFX'` to `'EFX'`. Verify the scene still runs headless (no API
  change) and the file no longer contains `EmotionFX`.
- [x] 4.2 Re-capture `tests/goldens/text_basic/golden.png` on the Linux
  llvmpipe verification server using the AGENTS.md capture recipe. Verify
  `git status` shows only that one golden image changed and it renders the new
  string.
- [x] 4.3 Run the golden harness for every scene (native, display-backed) and
  confirm no other golden image differs. Verify any additional diff is treated
  as a regression, not re-baselined.

## 5. Verification gate

- [x] 5.1 Run the headless Linux suite (`ctest --test-dir build -E golden
  --output-on-failure`) and confirm smoke + unit tests pass.
- [x] 5.2 Run `python3 tools/verify_remote.py all <branch>` on the SSH
  verification server and confirm the native golden suite, Emscripten golden
  suite, and gallery checks are green.
- [x] 5.3 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the
  four-target gate (Linux → Windows → macOS → Emscripten) is green, including
  the `gen_prelude.py --check` and `docs:check` steps.

## 6. Repository rename and hosting

- [ ] 6.1 Merge the branch into `main` and push (triggers the Pages deploy).
  Verify `git log` on `main` includes the change.
- [ ] 6.2 Rename the GitHub repository with `gh repo rename efx`. Verify
  `gh repo view mode777/efx` resolves and the old `mode777/emotion-fx` URL
  still redirects.
- [ ] 6.3 Update the local remote (`git remote set-url origin
  https://github.com/mode777/efx.git`). Verify `git fetch` succeeds.
- [ ] 6.4 Confirm the gallery redeploys at `https://mode777.github.io/efx/`
  after the next `main` push; verify the README links resolve and the gallery
  loads with its bundled player. Record the old Pages path as intentionally
  broken if GitHub does not redirect it.
- [ ] 6.5 Optionally set the repo `description` and `homepageUrl` on GitHub to
  the new EFX identity.

## 7. Archive

- [ ] 7.1 Archive the change (`npx openspec archive rebrand-to-efx`) once the
  gate is green and the rename is confirmed, and push the archive.
