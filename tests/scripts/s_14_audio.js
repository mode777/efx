/* F14 audio portable script: exercises the efx.audio binding (static and
 * streamed sources through one playback verb). Device-agnostic: it asserts
 * loads, types, and errors and that handles behave, without requiring an audio
 * device (so it is identical on desktop --script, desktop root, the Node web
 * harness, and a pre-gesture browser). */
var data = efx.audio.loadAudioData('tone.wav');
if (!data || typeof data.destroy !== 'function') throw new Error('loadAudioData');

var a = efx.audio.playAudio(data, { volume: 0.5, pan: -1 });
if (a !== null) {
    if (typeof a.playing !== 'boolean') throw new Error('audio playing type');
    if (typeof a.paused !== 'boolean') throw new Error('audio paused type');
    if (Math.abs(a.volume - 0.5) > 1e-6) throw new Error('volume getter');
    a.volume = 0.2;
    if (Math.abs(a.volume - 0.2) > 1e-6) throw new Error('volume setter');
    a.pan = 0.5;
    a.pitch = 1.5;
    a.loop = true;
    a.pause();
    a.resume();
    a.stop();
    a.destroy();
    a.destroy();
}

var stream = efx.audio.loadAudioStream('tone.mp3');
if (!stream || typeof stream.destroy !== 'function') throw new Error('loadAudioStream');
var m = efx.audio.playAudio(stream, { loop: true, volume: 0.3 });
if (!m || typeof m.stop !== 'function') throw new Error('stream handle');
if (typeof m.playing !== 'boolean') throw new Error('stream playing type');
m.volume = 0.4;
m.pause();
m.resume();
m.loop = false;
m.stop();
m.destroy();
stream.destroy();

if (typeof efx.audio.volume !== 'number') throw new Error('master getter');
efx.audio.volume = 0.5;
if (Math.abs(efx.audio.volume - 0.5) > 1e-6) throw new Error('master setter');

var e1 = 0;
try { efx.audio.loadAudioData(5); } catch (e) { e1 = (e instanceof TypeError) ? 1 : 2; }
if (e1 !== 1) throw new Error('path type');

var e2 = 0;
try { efx.audio.loadAudioData('nope.wav'); } catch (e) { e2 = (e instanceof Error) ? 1 : 2; }
if (e2 !== 1) throw new Error('missing audio');

var e3 = 0;
try { efx.audio.playAudio(data, { bogus: 1 }); } catch (e) { e3 = (e instanceof TypeError) ? 1 : 2; }
if (e3 !== 1) throw new Error('unknown option');

var e4 = 0;
try { efx.audio.playAudio({}); } catch (e) { e4 = (e instanceof TypeError) ? 1 : 2; }
if (e4 !== 1) throw new Error('bad source');

var e5 = 0;
try { efx.audio.volume = -1; } catch (e) { e5 = (e instanceof RangeError) ? 1 : 2; }
if (e5 !== 1) throw new Error('negative master');

data.destroy();
data.destroy();

efx.log('s-14-audio-ok');

efx.quit(0);
