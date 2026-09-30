# Spec Delta

## Purpose

The engine's audio layer: a vendored cross-target playback stack and a
dependency-free pure-C mixer that streams one background-music source and
mixes a fixed bank of overlapping sound effects, so scripts can play audio
without ever touching channels, buses, or buffers.

## ADDED Requirements

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
resampling, and streaming. The script-facing surface SHALL NOT expose channels,
buses, send/return routing, sample rates, sample buffers, or a mixing graph,
and SHALL NOT require the author to allocate or free voices. A script SHALL be
able to start background music and start a sound effect without any knowledge
of the mixer's internal structure.

#### Scenario: Author plays audio without channel management

- **WHEN** a script calls the background-music and sound-effect entry points
- **THEN** it passes only a resource path and an optional options object, and never a channel, bus, or buffer

#### Scenario: No mixing graph is script-visible

- **WHEN** the script API reference and namespace are inspected
- **THEN** no function or property exposes channels, buses, sends, or per-voice mixing state

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

### Requirement: Streaming background music

The engine SHALL provide one streamed background-music source that decodes a
long resource incrementally rather than holding the whole decoded file in
memory. The source SHALL support start, loop (on or off), pause, resume, stop,
and volume control, and SHALL expose whether it is currently playing. The
engine SHALL keep the decode buffer filled ahead of playback so ordinary
main-thread frame work does not interrupt the stream. Only one background-music
stream SHALL be active at a time; starting a new one SHALL replace the current
one.

#### Scenario: Music streams without full decode

- **WHEN** a long MP3 is started as background music
- **THEN** playback begins without the entire decoded file being resident in memory

#### Scenario: Music loops seamlessly at the buffer boundary

- **WHEN** looping background music reaches the end of the resource
- **THEN** it continues from the beginning without stopping the stream

#### Scenario: Music controls affect the active stream

- **WHEN** a script pauses, resumes, changes the volume of, or stops the background music
- **THEN** the active stream's audibility and reported playing state change accordingly

#### Scenario: Starting new music replaces the old

- **WHEN** background music is started while another track is already playing
- **THEN** the previous track stops and the new one becomes the active stream

### Requirement: Polyphonic sound-effect voice bank

The engine SHALL mix a fixed bank of simultaneously playable sound-effect
voices (32 voices) from fully-decoded sample buffers, so short sounds overlap
without the script allocating channels. Each playing effect SHALL expose a
handle with stop, a read-only playing state, and read-write volume, pan, and
pitch, plus an optional loop flag at start. When all voices are busy, the
engine SHALL apply a deterministic voice-stealing policy and SHALL NOT drop a
new request silently without the policy having been applied. Replaying the same
resource SHALL be cheap after its first load (decoded data is cached by
resource path).

#### Scenario: Effects overlap

- **WHEN** a script starts several effects in the same frame
- **THEN** each is audible simultaneously up to the 32-voice limit, with no channel argument from the script

#### Scenario: Voice limit is enforced deterministically

- **WHEN** more effects are started than there are free voices
- **THEN** the engine steals a voice according to the documented policy, and the outcome is identical for identical inputs

#### Scenario: Effect handle controls playback

- **WHEN** a script stops a playing effect, or changes its volume, pan, or pitch
- **THEN** the change takes effect on that voice and its playing state reflects reality

#### Scenario: Repeated effect is cached

- **WHEN** the same effect resource is started many times
- **THEN** it is decoded once and subsequent starts reuse the cached decoded data

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

- **WHEN** a script starts music or an effect with no audio device present
- **THEN** the calls do not crash and the engine reports that audio is not currently playing

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
