# Spec Delta: audio

## REMOVED Requirements

### Requirement: Web autoplay unlock

**Reason**: The requirement's script-facing path — an explicit
`efx.audio.resume()` entry point — is strictly dominated by engine-managed
unlock. A script can only call it from `update`/`render` hooks or callbacks,
which are never inside a browser user gesture, so the call fails and scripts
resort to flag-and-retry dances. The engine's input path receives
key-down/mouse-down inside the real DOM gesture and can unlock reliably on
the first try; keeping the script verb would preserve a dead API surface.

**Migration**: Scripts delete their `resume()` calls, unlock flags, and
try/catch retry blocks; the engine unlocks on the first keyboard or pointer
input. Playback handles keep their own `pause()`/`resume()` verbs, which are
unrelated to context unlock. Documented limitation: browsers do not treat
gamepad input as a user gesture, so a strictly gamepad-only web session
still needs one keyboard or pointer event before audio can start.

## ADDED Requirements

### Requirement: Autoplay unlock

On Emscripten, where the browser blocks audio until a user gesture, the
engine SHALL unlock audio by itself on the first keyboard key-down or mouse
button-down after script start, driven from the platform input path so the
unlock happens inside the real user gesture. No script-facing unlock or
audio-context resume entry point SHALL exist on the `efx.audio` namespace.
On desktop and other non-browser targets, or where no audio device is
available, the unlock path SHALL be a no-op. Audio requested before unlock
SHALL either be deferred or reported as not playing, consistently across the
desktop and web bindings. Browsers do not count gamepad input as a user
gesture; this limitation SHALL be documented rather than worked around.

#### Scenario: First input unlocks audio

- **WHEN** the page loads, the script starts music, and the user then
  provides any keyboard or pointer input
- **THEN** audio begins playing from that point with no script-side unlock
  call, flag, or retry logic

#### Scenario: No script-facing unlock entry point

- **WHEN** the `efx.audio` namespace is enumerated after this change
- **THEN** no `resume` or unlock member exists on it, while an `Audio`
  playback handle still exposes `pause()`/`resume()` for its own playback

#### Scenario: Pre-unlock behavior is consistent

- **WHEN** audio is requested before unlock on web
- **THEN** the observable playing state matches the documented behavior and
  is consistent with the desktop binding's no-device behavior

#### Scenario: No-device targets are unaffected

- **WHEN** the script runs on a desktop or headless target with no audio
  device and inputs keys or clicks
- **THEN** the unlock path is a no-op and playback calls keep their existing
  soft-fail behavior
