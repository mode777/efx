    /* video-playback (ADR 0063): the Video class and the efx.video
     * namespace. openVideo itself is installed by the shared prelude
     * (ADR 0049) through the natives object (audio.js fragment). */
    api.video = {};

    var EfxVideo = __efxResourceClass('EfxVideo', {
        typeMsg: 'expected a Video',
        init: function (id) {
            this.__id = id;
        },
        destroy: function () {
            bridge['_efx_bridge_video_destroy'](this.__id);
        },
        methods: {
            play: function () {
                bridge['_efx_bridge_video_play'](this.__id);
            },
            pause: function () {
                bridge['_efx_bridge_video_pause'](this.__id);
            },
        },
        getters: {
            currentTime: {
                get: function () {
                    return bridge['_efx_bridge_video_time'](this.__id);
                },
            },
            duration: {
                get: function () {
                    return bridge['_efx_bridge_video_duration'](this.__id);
                },
            },
            width: {
                get: function () {
                    return bridge['_efx_bridge_video_width'](this.__id);
                },
            },
            height: {
                get: function () {
                    return bridge['_efx_bridge_video_height'](this.__id);
                },
            },
            ended: {
                get: function () {
                    return !!bridge['_efx_bridge_video_ended'](this.__id);
                },
            },
            loop: {
                get: function () {
                    return !!bridge['_efx_bridge_video_get_loop'](this.__id);
                },
                set: function (v) {
                    bridge['_efx_bridge_video_set_loop'](this.__id, v ? 1 : 0);
                },
            },
            muted: {
                get: function () {
                    return !!bridge['_efx_bridge_video_get_muted'](this.__id);
                },
                set: function (v) {
                    bridge['_efx_bridge_video_set_muted'](this.__id, v ? 1 : 0);
                },
            },
            texture: {
                get: function () {
                    var handle = bridge['_efx_bridge_video_texture'](this.__id);
                    if (!handle) {
                        throw new TypeError('using a destroyed resource');
                    }
                    /* engine-owned: scripts cannot destroy it and the GC
                       backstop skips it — the video releases it (design D4) */
                    return new EfxTexture(handle, true);
                },
            },
        },
    });
