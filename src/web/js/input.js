    /* F9 input namespaces (desktop parity: one C source for names/state) */
    function makeInputRegister(list) {
        return function (fn) {
            if (typeof fn !== 'function') {
                throw new TypeError('hook must be a function');
            }
            var entry = { fn: fn, active: true };
            list.push(entry);
            return function () {
                entry.active = false;
            };
        };
    }
    function __keyId(name) {
        if (typeof name !== 'string') {
            throw new TypeError('keyboard query requires a key name');
        }
        var p = __efxAllocCStr(name);
        var id = bridge['_efx_bridge_key_id'](p);
        bridge['_free'](p);
        if (id < 0) {
            throw new TypeError('unknown key');
        }
        return id;
    }
    function __buttonId(name) {
        if (typeof name !== 'string') {
            throw new TypeError('mouse query requires a button name');
        }
        var p = __efxAllocCStr(name);
        var id = bridge['_efx_bridge_button_id'](p);
        bridge['_free'](p);
        if (id < 0) {
            throw new TypeError('unknown mouse button');
        }
        return id;
    }
    api.keyboard = {
        isDown: function (key) {
            return !!bridge['_efx_bridge_key_down'](__keyId(key));
        },
        isPressed: function (key) {
            return !!bridge['_efx_bridge_key_pressed'](__keyId(key));
        },
        isReleased: function (key) {
            return !!bridge['_efx_bridge_key_released'](__keyId(key));
        },
        onDown: makeInputRegister(st.keyboardDown),
        onUp: makeInputRegister(st.keyboardUp),
        onChar: makeInputRegister(st.keyboardChar),
    };
    api.mouse = {
        isDown: function (b) {
            return !!bridge['_efx_bridge_button_down'](__buttonId(b));
        },
        isPressed: function (b) {
            return !!bridge['_efx_bridge_button_pressed'](__buttonId(b));
        },
        isReleased: function (b) {
            return !!bridge['_efx_bridge_button_released'](__buttonId(b));
        },
        onDown: makeInputRegister(st.mouseDown),
        onUp: makeInputRegister(st.mouseUp),
        onMove: makeInputRegister(st.mouseMove),
        onWheel: makeInputRegister(st.mouseWheel),
    };
    Object.defineProperty(api.mouse, 'position', {
        get: function () {
            return [bridge['_efx_bridge_mouse_x'](), bridge['_efx_bridge_mouse_y']()];
        },
    });
    Object.defineProperty(api.mouse, 'x', {
        get: function () { return bridge['_efx_bridge_mouse_x'](); },
    });
    Object.defineProperty(api.mouse, 'y', {
        get: function () { return bridge['_efx_bridge_mouse_y'](); },
    });
    Object.defineProperty(api.mouse, 'delta', {
        get: function () {
            return [bridge['_efx_bridge_mouse_dx'](), bridge['_efx_bridge_mouse_dy']()];
        },
    });
    Object.defineProperty(api.mouse, 'wheel', {
        get: function () {
            return [bridge['_efx_bridge_wheel_dx'](), bridge['_efx_bridge_wheel_dy']()];
        },
    });
    api.window = {};
    Object.defineProperty(api.window, 'size', {
        get: function () {
            return [bridge['_efx_bridge_window_width'](),
                    bridge['_efx_bridge_window_height']()];
        },
    });
    Object.defineProperty(api.window, 'width', {
        get: function () { return bridge['_efx_bridge_window_width'](); },
    });
    Object.defineProperty(api.window, 'height', {
        get: function () { return bridge['_efx_bridge_window_height'](); },
    });
    Object.defineProperty(api.window, 'dpiScale', {
        get: function () { return bridge['_efx_bridge_window_dpi'](); },
    });

    /* F13 gamepad namespace (desktop parity: same C core, same errors) */
    api.gamepad = {
        get: function (index) {
            if (typeof index !== 'number') {
                throw new TypeError('gamepad.get requires an index');
            }
            var i = index | 0;
            if (!bridge['_efx_bridge_gamepad_connected'](i)) {
                return null;
            }
            return __efxGamepadView(i);
        },
        onConnect: makeInputRegister(st.gamepadConnect),
        onDisconnect: makeInputRegister(st.gamepadDisconnect),
    };
    Object.defineProperty(api.gamepad, 'count', {
        get: function () { return bridge['_efx_bridge_gamepad_count'](); },
    });

    /* ---------------------------------------------- F12 physics */

    var physBodies = new Map();
    var physCharacters = new Map();

