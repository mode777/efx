# Spec Delta

## MODIFIED Requirements

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

## REMOVED Requirements

### Requirement: Streaming background music

**Reason**: The singleton background-music stream conflated decode strategy
with a playback role. It is replaced by streamed sources played through the
unified playback API, which additionally allows more than one stream to mix
(for example a crossfade).

**Migration**: Replace `efx.audio.playBackgroundMusic(path, opts)` with
`efx.audio.playAudio(efx.audio.loadAudioStream(path), opts)`; the returned
`Audio` handle provides `pause`, `resume`, `stop`, and `volume` in place of the
old `Music` methods. `efx.audio.stopBackgroundMusic()` becomes
`handle.stop()`.

### Requirement: Polyphonic sound-effect voice bank

**Reason**: The dedicated effect role is replaced by a single playback bank
that mixes both static and streamed sources with identical per-handle controls;
"effect" is no longer a distinct engine concept.

**Migration**: Replace `efx.audio.playAudioEffect(path, opts)` with
`efx.audio.playAudio(efx.audio.loadAudioData(path), opts)` (decoded data may be
retained and reused by the script), and `efx.audio.playSound(data, opts)` with
`efx.audio.playAudio(data, opts)`; the returned `Audio` handle provides
`volume`/`pan`/`pitch`/`loop`/`playing`/`stop`.

## ADDED Requirements

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
