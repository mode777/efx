# Spec Delta

## MODIFIED Requirements

### Requirement: Fixed milestone order

The roadmap SHALL define the F1–F8 ordered feature ladder — F1 (player
skeleton), F2 (2D layer + display list + verification harness), F3 (3D core),
F4 (lighting + Phong, split F4a/F4b), F5 (render targets + post FX), F6
(resource packaging + glTF 2.0 asset import + REPL), F7 (skinning +
animation), F8 (high-level JS layer + text) — plus explicitly
declared **orthogonal** milestones that are not inserted into the F3–F8
dependency chain: F9 (input — keyboard + mouse query and event API), whose
only predecessors are the F1–F2 window/frame loop and dual script bindings;
F10 (script modules — CommonJS), whose only predecessors are the F1–F2
dual script bindings and the F6a dir/zip resource provider; F11 (particles
+ billboards — CPU particle systems, world-space billboards, batched 2D
sprites), whose only predecessors are the F2 2D quad/display-list contract and
the F3 3D camera/depth core (and F6a for particle textures loaded from files);
and F12
(collision + character + impulse dynamics — a bespoke pure-C collision world,
a kinematic capsule character controller, linear impulse dynamics, and
spatial queries), whose only predecessors are the F3 3D camera/math/mesh core
and the F6a/F6b resource + glTF import (for collision meshes loaded from
files). F9, F10, F11, and F12 MAY be
implemented once their predecessors' gates have passed, independently of each
other and of the remaining F3–F8 milestones; F10 is an explicit enabler of
F8's pure-JS high-level layer. F6 asset import
SHALL cover glTF 2.0 payloads: meshes, images (textures), skins, and
animation clips. Each milestone SHALL build only on
capabilities delivered by its predecessors (for F9–F11, the F1–F2
foundation, plus F6a for F10 and F11, plus F3 for F11; for F12, the F3 core
plus F6a/F6b), and the milestone order MUST NOT be
reordered without a change to this capability.

#### Scenario: Locating a feature in the ladder
- **WHEN** a future feature proposal is drafted
- **THEN** the roadmap assigns it exactly one milestone, and that milestone's
  predecessor list is unambiguous

#### Scenario: Out-of-order proposal
- **WHEN** a proposal implements functionality that belongs to a milestone whose
  predecessors have not passed their verification gate
- **THEN** the proposal is out of roadmap order and MUST NOT proceed to
  implementation until the predecessor's gate passes

#### Scenario: Orthogonal milestone proceeds early
- **WHEN** a proposal implements F9 (input), F10 (script modules), F11
  (particles + billboards), or F12 (collision + character + impulse dynamics)
  after its predecessor gates have passed but before F7 or F8 complete
- **THEN** the proposal is in roadmap order, because those milestones' only
  predecessors are F1–F2 (and F6a for F10, plus F6a and F3 for F11, plus F3
  and F6a/F6b for F12)

### Requirement: Verification gate per milestone

Every milestone SHALL define a verification strategy that must pass on all four
target platforms (Windows, Linux, macOS, Emscripten) before the next milestone
starts. F1 verification SHALL be based on the build matrix plus script-mode
smoke tests with exit-code checks. From F2 onward, rendering milestones SHALL
be verified with a golden-image pixel-diff harness introduced as a first-class
F2 deliverable, complemented by unit tests for non-visual logic; F4 lighting
math and F7 skinning math SHALL additionally be verified against CPU reference
implementations. Non-rendering milestones SHALL NOT require a golden-image
gate: F9 (input) SHALL be verified by headless unit tests over the C input
core driven through a deterministic simulation seam plus a script-level
simulation harness, F10 (script modules) SHALL be verified by portable
module smoke scripts run through both the desktop and web runtimes
(ctest + the cross-runtime comparison), covering resolution, caching, cycles,
interop, JSON modules, and error behavior, F11 (particles + billboards) SHALL
be verified by headless unit tests over the deterministic CPU particle
simulation (against a CPU reference) and the billboard math, a portable script
smoke case on the desktop and web runtimes, and a golden-image scene for the
billboard and particle render paths, and F12 (collision + character +
impulse dynamics) SHALL be verified by headless unit tests over the
dependency-free C collision, dynamics, and character core (direct narrowphase
cases, invariants, scenarios, determinism, and stress) plus a portable
script-level simulation harness run through both the desktop and web runtimes,
with no golden-image test required.

#### Scenario: F1 gate
- **WHEN** F1 completes
- **THEN** the player binary builds on all four targets and a scripted smoke
  test that crosses the JS/C boundary passes with a zero exit code on each

#### Scenario: Rendering milestone gate
- **WHEN** a rendering milestone (F2 or later) completes
- **THEN** its golden-image comparisons pass within the defined pixel tolerance
  on all four targets, and its unit tests pass

#### Scenario: Input milestone gate
- **WHEN** F9 (input) completes
- **THEN** its headless unit tests and script-level simulation harness pass on
  all four targets, and no golden-image test is required

#### Scenario: Script modules milestone gate
- **WHEN** F10 (script modules) completes
- **THEN** its portable module smoke scripts pass through both runtimes on all
  four targets, the cross-runtime comparison shows identical results, and no
  golden-image test is required

#### Scenario: Particles and billboards milestone gate
- **WHEN** F11 (particles + billboards) completes
- **THEN** its headless simulation and billboard unit tests pass, its platform
  script smoke case passes on all four targets, and its golden-image scene
  passes within tolerance on all four targets

#### Scenario: Physics milestone gate
- **WHEN** F12 (collision + character + impulse dynamics) completes
- **THEN** its headless unit tests over the dependency-free C core pass, its
  portable script-level simulation harness passes through both runtimes on all
  four targets with identical results, and no golden-image test is required
