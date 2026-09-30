# audio Specification

## Purpose
The engine's audio layer: a vendored cross-target playback stack and a
dependency-free pure-C core that plays static (fully-decoded) and streamed
audio sources through one fixed bank of overlapping playback voices with a
single master output gain, so scripts can load, play, and control audio
without ever touching channels, buses, or buffers.

## Requirements

### Requirement: Vendored audio stack with four-target parity

The engine SHALL obtain audio output through a single vendored playback
backend (`sokol_audio`, from the already-pinned Sokol snapshot) and SHALL
decode audio through a single vendored decoder snapshot covering WAV and MP3.
Both SHALL be pinned source snapshots recorded in the vendor table with their
license, revision, and source origin, and the build SHALL never fetch them
from the network. The playback backend SHALL be confined to the platform layer
behind an engine-owned interface; scripts and the pure-C core SHALL NOT depend
on its types, functions, or lifecycle. The decoder SHALL support Windows,
Linux, macOS, and Emscripten and SHALL have no external library dependency.

#### Scenario: Decoding is backend-agnostic to callers

- **WHEN** the audio core decodes a sound or is exercised by a headless test
- **THEN** it uses only engine-owned decoding state and never a backend struct, enum, or function

#### Scenario: All four targets can decode and output

- **WHEN** the player runs on Windows, Linux, macOS, or Emscripten
- **THEN** WAV and MP3 resources decode and play through that target's audio backend

#### Scenario: Build is offline

- **WHEN** the project is configured and built without network access
- **THEN** the audio backend and decoders resolve from the vendored snapshots

### Requirement: Engine-owned mixing with no script-visible channels

The engine SHALL own all mixing, voice allocation, panning, pitch, envelopes,
resampling, and streaming. The script-facing surface SHALL expose only a
read-write per-playback `volume` (with `pitch`/`pan`/`loop`) on the playback
handle and a single read-write master output gain on `efx.audio`; it SHALL NOT
expose channels, buses, send/return routing, sample rates, sample buffers, or a
mixing graph, and SHALL NOT require the author to allocate or free voices. A
script SHALL be able to start a static or streamed source through one playback
entry point without any knowledge of the mixer's internal structure.

#### Scenario: Author plays audio without channel management

- **WHEN** a script starts a static or streamed source through the playback entry point
- **THEN** it passes a loaded source and an optional options object, and never a channel, bus, buffer, or voice index

#### Scenario: No mixing graph is script-visible

- **WHEN** the script API reference and namespace are inspected
- **THEN** no function or property exposes channels, buses, sends, or per-voice mixing state, and the only grouping control is the single master output gain

#### Scenario: Volume is a handle property

- **WHEN** a script changes the loudness of a playing sound after it has started
- **THEN** it sets a property on that playback handle (or the master gain), not a parameter of a play call

### Requirement: Decoded PCM only; no sequenced or modular formats

The engine SHALL play only decoded PCM audio — a fully-decoded sample buffer or
an incrementally-decoded stream. It SHALL NOT implement, load, or execute
modular/sequenced formats that separate a score or sequencer program from
instrument samples (for example PSF2/PSF, tracker modules, or MIDI), because
those formats are obsolete and lack modern authoring and content. A resource
that is not WAV or MP3 SHALL be rejected with an error rather than guessed at.

#### Scenario: Sequenced format is rejected

- **WHEN** a script attempts to load a PSF2, tracker module, or MIDI resource
- **THEN** the load fails with an error and the engine does not attempt to interpret a score or instrument program

#### Scenario: Decoded formats play

- **WHEN** a WAV or MP3 resource is loaded
- **THEN** it plays as decoded PCM without any score/sequencer interpretation

### Requirement: Deterministic pure-C mixing core with a headless seam

The mixing core SHALL be pure C with no dependency on the playback backend or
the script runtime, and SHALL expose a deterministic mixing entry point that
produces a block of output samples from current engine state given an explicit
frame count. The core SHALL be drivable with no audio device and no threads, so
headless tests can inject requests, mix, and assert sample values exactly. The
core SHALL also expose a deterministic injection/control seam used by tests
that is not script-visible.

#### Scenario: Headless mix is deterministic

- **WHEN** the same requests and parameters are applied twice and the core is asked to mix the same number of frames
- **THEN** both mixes produce identical output samples

#### Scenario: Core needs no device

- **WHEN** the headless unit-test build runs with no audio device present
- **THEN** the mixing core loads, mixes, and is asserted on without error

### Requirement: WAV and MP3 decoding with sample-rate conversion

The engine SHALL decode WAV (integer PCM and float) and MP3 resources from the
resource provider. Decoded data SHALL be converted to the engine's internal
sample format and resampled from the file's sample rate to the device sample
rate using linear interpolation. Playback pitch SHALL be expressed as a
playback-rate multiplier over that conversion. A malformed, truncated, or
unsupported resource SHALL fail with an error; the engine SHALL NOT crash or
produce undefined output for it.

#### Scenario: WAV and MP3 both load

- **WHEN** a script loads a WAV resource and an MP3 resource
- **THEN** both decode successfully and report a usable duration/frame count

#### Scenario: File rate differs from device rate

- **WHEN** a resource whose sample rate differs from the device sample rate is played
- **THEN** it plays at the correct musical speed and pitch, not too fast or too slow

#### Scenario: Corrupt resource fails cleanly

- **WHEN** a script loads a truncated or non-audio resource as audio
- **THEN** the load throws an error and no partial or garbage playback occurs

### Requirement: No-device soft-fail

When no audio device is available (for example a headless CI machine or a
browser before the user gesture unlocks audio), the engine SHALL continue to
run rather than aborting. Audio setup failure SHALL NOT be fatal to the player;
audio-dependent calls SHALL remain safe (no crash, no undefined behavior) and
the engine SHALL report the unavailable/not-yet-unlocked state.

#### Scenario: Headless run does not crash

- **WHEN** the player runs on a machine with no audio device
- **THEN** it starts, runs its script, and exits normally, with audio silently unavailable

#### Scenario: Audio calls remain safe without a device

- **WHEN** a script starts playback of a static or streamed source with no audio device present
- **THEN** the calls do not crash and every playback handle reports not playing

### Requirement: Web autoplay unlock

On Emscripten, where the browser blocks audio until a user gesture, the engine
SHALL resume/unlock audio on the first user input or through an explicit
script call, without requiring the author to manage the browser's audio
context directly. Audio requested before unlock SHALL either be deferred or
reported as not playing, consistently across the desktop and web bindings.

#### Scenario: First input unlocks audio

- **WHEN** the page loads, the script starts music, and the user then provides any keyboard or mouse input
- **THEN** audio begins playing from that point without an additional explicit call

#### Scenario: Explicit resume is available

- **WHEN** a script calls the audio resume entry point after a user gesture
- **THEN** audio is unlocked and pending playback proceeds

#### Scenario: Pre-unlock behavior is consistent

- **WHEN** audio is requested before unlock on web
- **THEN** the observable playing state matches the documented behavior and is consistent with the desktop binding's no-device behavior

### Requirement: Static and streamed audio source kinds

The engine SHALL expose exactly two audio source kinds, loaded separately from
playback (load first, then play): a **static** source holding the fully decoded
sample buffer in memory, and a **streamed** source that decodes a long resource
incrementally and keeps the decode buffer filled ahead of playback. Both SHALL
read WAV or MP3 resources from the resource provider. A static source SHALL be
re-playable and SHALL be usable by multiple simultaneous playbacks without
re-decoding. Multiple streamed sources MAY play concurrently, up to a
documented fixed cap of at least two, so tracks can overlap or crossfade.

#### Scenario: Long stream plays without full decode

- **WHEN** a long MP3 is loaded as a streamed source and played
- **THEN** playback begins without the entire decoded file being resident in memory, and the decoder stays ahead of playback

#### Scenario: Static source backs overlapping playbacks

- **WHEN** the same static source is played more than once at the same time
- **THEN** each playback is independent and no additional decode of the resource occurs

#### Scenario: Concurrent streams mix

- **WHEN** two streamed sources are played at the same time
- **THEN** both are audible simultaneously, up to the documented stream cap

#### Scenario: Stream cap is enforced

- **WHEN** more streamed sources are started than the documented cap allows
- **THEN** the engine applies a deterministic policy and reports the outcome rather than silently dropping the request

### Requirement: Unified playback handle

The script API SHALL provide one playback entry point that accepts either
source kind and returns a single playback-handle class. The handle SHALL expose
a read-only `playing` and `paused`, read-write `volume`, `pitch`, `pan`, and
`loop`, and `stop`, `pause`, `resume`, and `destroy`. Options passed at start
SHALL set only initial values; later changes SHALL be made on the handle, so a
script can fade, retune, or re-pan a sound while it plays. The engine SHALL mix
a fixed bank of simultaneously playable handles and SHALL apply a deterministic
policy when the bank is full.

#### Scenario: Handle controls an active playback

- **WHEN** a script changes the volume, pitch, pan, or loop of a playing handle, or pauses, resumes, or stops it
- **THEN** the change takes effect on that playback and `playing`/`paused` reflect reality

#### Scenario: Fade is a sequence of handle writes

- **WHEN** a script starts a sound at zero volume and raises the handle's volume over several frames
- **THEN** the sound fades in without any engine fade parameter

#### Scenario: Overlapping playbacks

- **WHEN** a script plays several sources in the same frame
- **THEN** each is audible simultaneously up to the voice-bank limit, with no channel or voice argument from the script

#### Scenario: Voice limit is enforced deterministically

- **WHEN** more playbacks are started than there are free voices
- **THEN** the engine applies the documented steal policy and the outcome is identical for identical inputs

### Requirement: Master output gain

The script API SHALL expose a single read-write master output gain for
`efx.audio`, scaling all output. It SHALL be the only grouping volume control;
per-source buses and channels SHALL NOT exist. Setting a negative value SHALL
throw `RangeError` and leave the gain unchanged.

#### Scenario: Master gain scales all playback

- **WHEN** a script lowers the master output gain
- **THEN** every playing source becomes quieter, and raising it restores loudness without restarting playback

#### Scenario: Negative master gain is rejected

- **WHEN** a script assigns a negative value to the master output gain
- **THEN** the assignment throws `RangeError` and the gain is unchanged
