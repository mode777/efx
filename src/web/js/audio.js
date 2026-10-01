    /* --------------------------------------------- F14 audio */
    function __efxAudioOpts(opts, known, where) {
        if (opts === undefined || opts === null) {
            return {};
        }
        if (typeof opts !== 'object') {
            throw new TypeError(where + ' options must be an object');
        }
        var out = {};
        for (var k in opts) {
            if (Object.prototype.hasOwnProperty.call(opts, k)) {
                if (known.indexOf(k) < 0) {
                    throw new TypeError("unknown " + where + " option '" + k + "'");
                }
                out[k] = opts[k];
            }
        }
        return out;
    }

    var EfxAudioData = __efxResourceClass('EfxAudioData', {
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_audio_data_destroy'](this.__id);
        },
    });

    var EfxAudioStream = __efxResourceClass('EfxAudioStream', {
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_audio_stream_destroy'](this.__id);
        },
    });

    /* F14: a playback handle. Its members tolerate a dead id (queries read
       false, verbs no-op) rather than throwing, so they are all raw. */
    var EfxAudio = __efxResourceClass('EfxAudio', {
        init: function (id, volume, pan, pitch, loop) {
            this.__id = id;
            this.__volume = volume;
            this.__pan = pan;
            this.__pitch = pitch;
            this.__loop = loop;
        },
        destroy: function () {
            bridge['_efx_bridge_audio_handle_destroy'](this.__id);
        },
        methods: {
            stop: { raw: true, fn: function () {
                if (this.__alive) {
                    bridge['_efx_bridge_audio_handle_stop'](this.__id);
                }
            } },
            pause: { raw: true, fn: function () {
                if (this.__alive) {
                    bridge['_efx_bridge_audio_handle_pause'](this.__id, 1);
                }
            } },
            resume: { raw: true, fn: function () {
                if (this.__alive) {
                    bridge['_efx_bridge_audio_handle_pause'](this.__id, 0);
                }
            } },
        },
        getters: {
            playing: { raw: true, get: function () {
                return this.__alive &&
                    !!bridge['_efx_bridge_audio_handle_playing'](this.__id);
            } },
            paused: { raw: true, get: function () {
                return this.__alive &&
                    !!bridge['_efx_bridge_audio_handle_paused'](this.__id);
            } },
            volume: {
                raw: true,
                get: function () { return this.__volume; },
                set: function (v) {
                    var n = __efxFiniteNumber(v, 'volume');
                    if (n < 0) throw new RangeError('volume must be a non-negative number');
                    this.__volume = n;
                    bridge['_efx_bridge_audio_handle_set_volume'](this.__id, n);
                },
            },
            pan: {
                raw: true,
                get: function () { return this.__pan; },
                set: function (v) {
                    var n = __efxFiniteNumber(v, 'pan');
                    this.__pan = n;
                    bridge['_efx_bridge_audio_handle_set_pan'](this.__id, n);
                },
            },
            pitch: {
                raw: true,
                get: function () { return this.__pitch; },
                set: function (v) {
                    var n = __efxFiniteNumber(v, 'pitch');
                    if (n <= 0) throw new RangeError('pitch must be a positive number');
                    this.__pitch = n;
                    bridge['_efx_bridge_audio_handle_set_pitch'](this.__id, n);
                },
            },
            loop: {
                raw: true,
                get: function () { return this.__loop; },
                set: function (v) {
                    this.__loop = !!v;
                    bridge['_efx_bridge_audio_handle_set_loop'](this.__id, v ? 1 : 0);
                },
            },
        },
    });

    function __efxAudioPlay(playFn, src, opts) {
        var o = __efxAudioOpts(opts, ['volume', 'pan', 'pitch', 'loop'],
                               'playAudio');
        var volume = (o.volume === undefined) ? 1
            : __efxFiniteNumber(o.volume, 'volume');
        if (volume < 0) {
            throw new RangeError('volume must be a non-negative number');
        }
        var pan = (o.pan === undefined) ? 0 : __efxFiniteNumber(o.pan, 'pan');
        var pitch = (o.pitch === undefined) ? 1 : __efxFiniteNumber(o.pitch, 'pitch');
        if (pitch <= 0) pitch = 1;
        var loop = o.loop ? 1 : 0;
        var id = playFn(src.__id, volume, pan, pitch, loop);
        if (!id) return null;
        return new EfxAudio(id, volume, pan, pitch, !!o.loop);
    }

    api.audio = {
        loadAudioData: function (path) {
            if (typeof path !== 'string') {
                throw new TypeError('loadAudioData requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_audio_load_data'](p);
            bridge['_free'](p);
            if (!id) {
                throw new Error('cannot decode audio: ' + path);
            }
            return new EfxAudioData(id);
        },
        loadAudioStream: function (path) {
            if (typeof path !== 'string') {
                throw new TypeError('loadAudioStream requires a path string');
            }
            var p = __efxAllocCStr(path);
            var id = bridge['_efx_bridge_audio_load_stream'](p);
            bridge['_free'](p);
            if (!id) {
                throw new Error('cannot decode audio: ' + path);
            }
            return new EfxAudioStream(id);
        },
        playAudio: function (source, opts) {
            if (source instanceof EfxAudioData) {
                if (!source.__alive) throw new Error('AudioData was destroyed');
                return __efxAudioPlay(
                    bridge['_efx_bridge_audio_play_data'], source, opts);
            }
            if (source instanceof EfxAudioStream) {
                if (!source.__alive) throw new Error('AudioStream was destroyed');
                return __efxAudioPlay(
                    bridge['_efx_bridge_audio_play_stream'], source, opts);
            }
            throw new TypeError('playAudio requires an AudioData or AudioStream');
        },
        resume: function () {
            bridge['_efx_audio_request_resume']();
        },
    };
    Object.defineProperty(api.audio, 'volume', {
        get: function () {
            return bridge['_efx_audio_master_volume']();
        },
        set: function (v) {
            var n = __efxFiniteNumber(v, 'volume');
            if (n < 0) throw new RangeError('volume must be a non-negative number');
            bridge['_efx_audio_set_master_volume'](n);
        },
    });

    var whiteTex = null;
    Object.defineProperty(api, 'whiteTexture', {
        get: function () {
            if (!whiteTex) {
                var handle = bridge['_efx_bridge_white_texture']();
                if (!handle) {
                    throw new Error('white texture unavailable');
                }
                whiteTex = new EfxTexture(handle, true);
            }
            return whiteTex;
        },
    });

    /* engine-bundled pure-JS layer (F3 math + primitives, F10 CommonJS
       runtime): the same embedded source the desktop quickjs runtime
       evaluates (ADR 0022). The IIFE returns the module-runtime factory,
       which is instantiated per entry below with the web host-global
       shadow. The second wrapper parameter carries the binding-provided
       natives object (R22 spike, design D1); the web binding passes none
       yet, so the shared validators stay dormant here. */
    var preludeSrc = UTF8ToString(bridge['_efx_bridge_js_prelude']());
    st.createModuleRuntime = new Function('efx', 'natives', preludeSrc)(api, undefined);

    globalThis['efx'] = api;
    st.api = api;
    st.quitSentinel = new Object();
    return st;
}
