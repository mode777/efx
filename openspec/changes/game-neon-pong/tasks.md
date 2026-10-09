# Tasks

## 1. Sample scaffold and assets

- [x] 1.1 Create `gallery/samples/curated/game-neon-pong/` and copy the standard Kenney `font.ttf` (same CC0 provenance as `text-showcase/`); add its row to `gallery/samples/curated/CREDITS.md` ("same font as `text-showcase/`")
- [x] 1.2 Add the `manifest.json` entry (title, `Games` category, description noting the complete match loop and self-play) and regenerate the catalog with `node gallery/scripts/gen-catalog.mjs`; verify the sample appears in the generated catalog JSON

## 2. Core game loop

- [x] 2.1 Author `main.js` skeleton: 640×480 frame, clear color, module-level state machine constants (ATTRACT/SERVE/RALLY/POINT/WIN) and named tuning constants (paddle speed, ball speed ramp, serve delay, target score); verify it boots to a visible frame under a desktop player build against the sample directory
- [x] 2.2 Implement paddles and ball as quads with the neon palette, script AABB collision, angle-from-paddle-offset bounce, and per-hit speed ramp; verify a rally bounces with visibly varying angles and accelerating pace
- [x] 2.3 Implement scoring with `drawText` (two baked font sizes), serve delay after each point, win screen at the target score, and any-input restart into a fresh match; verify a full match to 7 plays and restarts
- [x] 2.4 Implement the imperfect AI paddle (capped speed, deadzone, per-rally aim error) and the two-player keyboard toggle (P key, W/S vs arrows); verify the AI is beatable and the toggle swaps control

## 3. Input and attract mode

- [x] 3.1 Implement player controls: mouse-Y tracking and ↑/↓ held-state with last-input-wins; verify both control paths feel responsive and don't fight after switching
- [x] 3.2 Implement the first-input edge detector (any key, any mouse button, first mouse move) and ATTRACT state driving both paddles with the AI; verify launch shows a live AI-vs-AI match that hands over the player paddle on first input

## 4. Verification

- [ ] 4.1 Run the sample in the built gallery and confirm the full loop (attract → match → win → restart) with no console or page errors; verify the gallery smoke run includes the sample and passes
- [ ] 4.2 Run `npx openspec validate "game-neon-pong" --type change --strict` and confirm it passes
- [ ] 4.3 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, then dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` (ADR 0020/0023) and confirm all targets are green
