# Tasks

## 1. Sample scaffold and assets

- [x] 1.1 Create `gallery/samples/curated/game-mini-golf/`, copy the standard Kenney `font.ttf`, and add its CREDITS row; add the `manifest.json` entry (`Games` category, description noting raycast aiming, the power meter, and nine procedural holes) and regenerate the catalog; verify the sample appears in the generated catalog JSON
- [x] 1.2 Author the nine-hole data table (`walls`, `ramps`, `tee`, `cup`, `par` per hole) with each hole fully enclosed by its walls; verify by inspection that no hole leaves an open side

## 2. Physics course

- [x] 2.1 Build the hole loader: static box bodies for walls, static triangle-mesh bodies for ramps, rendered meshes for both, `destroy()` of the previous hole's bodies on advance (ADR 0011/0012 discipline); verify consecutive hole loads leave no orphan bodies (physics `clear()`-then-rebuild sanity check in a debug branch)
- [x] 2.2 Create the dynamic ball (sphere, mass/friction/restitution tuned for roll) with stop detection (`|v| < ε` for N frames → aiming state) and a per-hole stroke limit that auto-advances; verify the ball rolls, bounces off walls, climbs ramps, and always comes to rest

## 3. Stroke play

- [x] 3.1 Implement cursor → ground-plane raycast aiming (camera-derived ray, plane hit, ball→hit direction clamped to a pitch band) with a rendered aim line; verify the aim line tracks the cursor from any camera position and never points steeply up
- [x] 3.2 Implement the hold-to-charge ping-pong power meter and release-to-fire `applyImpulse`; verify stroke strength follows meter phase and the meter renders as a 2D overlay bar
- [x] 3.3 Implement the cup as a sensor sphere with the speed gate (slow entry captures, fast rolls over), `velocity` zero on capture, sink pause, hole advance, and the hole-9 scorecard (strokes vs par per hole) with any-input restart; verify a full nine-hole round plays end to end

## 4. Presentation and attract mode

- [x] 4.1 Generate the grass texture with `createImageData`, light the course (1 directional + 1 point), render the blob shadow under the ball, and set the fixed-elevation lerped follow camera; verify the ball stays grounded visually on ramps and the camera follows without jitter
- [x] 4.2 Implement ATTRACT (scripted aim-at-cup with error, fire at ~70% meter) with first-input handover at the next AIM state; verify launch self-plays and mouse/key input takes over

## 5. Verification

- [x] 5.1 Run the built gallery sample through attract → a played round → scorecard restart with no console or page errors; verify the gallery smoke run includes the sample and passes
- [x] 5.2 Run `npx openspec validate "game-mini-golf" --type change --strict` and confirm it passes
- [x] 5.3 Commit, push the branch, run `python3 tools/verify_remote.py all <branch>`, then dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` (ADR 0020/0023) and confirm all targets are green
