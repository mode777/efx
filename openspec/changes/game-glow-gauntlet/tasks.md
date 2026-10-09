# Tasks

## 1. Assets

- [x] 1.1 Extend `gallery/scripts/gen-audio-assets.py` with a `game-glow-gauntlet` bank synthesizing a seamless looping `music.wav` and a `sting.wav`; verify `--check` passes and the files land in `gallery/samples/curated/game-glow-gauntlet/`
- [x] 1.2 Copy the standard Kenney `font.ttf` into the sample directory and add CREDITS rows (font, music provenance, synthesized sting)
- [x] 1.3 Add the `manifest.json` entry (`Games` category, description noting one-button hold-to-rise across key/mouse/gamepad and the music loop) and regenerate the catalog; verify the sample appears in the generated catalog JSON

## 2. Core game loop

- [x] 2.1 Author `main.js` skeleton: 640×480 frame, state machine (ATTRACT/RUN/DYING/GAME-OVER), seeded PRNG, named tuning constants (gravity, thrust, gate speed ramp, gap floor); verify boot under the player
- [x] 2.2 Implement the avatar as a quad with a `createImageData` glow texture and hold-to-rise physics (accelerate up while held, gravity when released); verify the feel is floaty and controllable
- [x] 2.3 Implement the gate pattern library and seeded spawner: patterns spaced by the PRNG, scrolling down past the fixed-x avatar, speed and gap width scaling with score toward the floor; verify runs get harder and seeded runs are reproducible
- [x] 2.4 Implement AABB death, the additive death burst, the `sting` one-shot, score = gates passed with session-best text, the game-over screen, and same-button restart; verify a death cycle scores, bursts, and restarts instantly

## 3. Input, music, and attract mode

- [x] 3.1 Map the single control to held state across Space, LMB, and a mapped gamepad face button (`efx.gamepad.get`); verify all three rise/release identically in one session
- [x] 3.2 Stream the music loop via `loadAudioStream` + `playAudio` with M mute and `[` / `]` volume (audio-showcase key convention); verify the loop plays, controls act, and web audio unlocks on first input
- [x] 3.3 Implement the attract heuristic (hold when below the next gap's center line) and first-input handover; verify launch self-plays visibly and the first input takes control

## 4. Verification

- [ ] 4.1 Run the built gallery sample through attract → play → death → restart with no console or page errors; verify the gallery smoke run includes the sample and passes
- [ ] 4.2 Run `npx openspec validate "game-glow-gauntlet" --type change --strict` and confirm it passes
- [ ] 4.3 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, then dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` (ADR 0020/0023) and confirm all targets are green
