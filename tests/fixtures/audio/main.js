/* F14 audio root-mode entry (same checks as tests/scripts/s_14_audio.js):
 * exercises efx.audio + the prelude's playAudioEffect against the fixtures
 * beside this file. Device-agnostic, so it passes with or without a device. */
var sd = efx.audio.loadSoundData('tone.wav');
if (!sd || typeof sd.destroy !== 'function') throw new Error('loadSoundData');

var s = efx.audio.playAudioEffect('tone.wav', { volume: 0.5 });
if (s !== null) {
    if (typeof s.playing !== 'boolean') throw new Error('sound playing type');
    if (Math.abs(s.volume - 0.5) > 1e-6) throw new Error('volume getter');
    s.volume = 0.2;
    if (Math.abs(s.volume - 0.2) > 1e-6) throw new Error('volume setter');
    s.pan = 0.5;
    s.pitch = 1.5;
    s.stop();
    s.destroy();
    s.destroy();
}

var m = efx.audio.playBackgroundMusic('tone.mp3', { loop: true });
if (!m || typeof m.setVolume !== 'function') throw new Error('music handle');
if (typeof m.playing !== 'boolean') throw new Error('music playing type');
m.setVolume(0.3);
m.pause();
m.resume();
m.stop();
m.destroy();

var e1 = 0;
try { efx.audio.loadSoundData(5); } catch (e) { e1 = (e instanceof TypeError) ? 1 : 2; }
if (e1 !== 1) throw new Error('path type');

var e2 = 0;
try { efx.audio.loadSoundData('nope.wav'); } catch (e) { e2 = (e instanceof Error) ? 1 : 2; }
if (e2 !== 1) throw new Error('missing audio');

var e3 = 0;
try { efx.audio.playSound(sd, { bogus: 1 }); } catch (e) { e3 = (e instanceof TypeError) ? 1 : 2; }
if (e3 !== 1) throw new Error('unknown option');

sd.destroy();

efx.log('s-14-audio-ok');
