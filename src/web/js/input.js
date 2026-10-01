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
        var id = bridge['_efx_input_key_id'](p);
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
        var id = bridge['_efx_input_button_id'](p);
        bridge['_free'](p);
        if (id < 0) {
            throw new TypeError('unknown mouse button');
        }
        return id;
    }
    api.keyboard = {
        isDown: function (key) {
            return !!bridge['_efx_input_key_is_down'](__keyId(key));
        },
        isPressed: function (key) {
            return !!bridge['_efx_input_key_is_pressed'](__keyId(key));
        },
        isReleased: function (key) {
            return !!bridge['_efx_input_key_is_released'](__keyId(key));
        },
        onDown: makeInputRegister(st.keyboardDown),
        onUp: makeInputRegister(st.keyboardUp),
        onChar: makeInputRegister(st.keyboardChar),
    };
    api.mouse = {
        isDown: function (b) {
            return !!bridge['_efx_input_button_is_down'](__buttonId(b));
        },
        isPressed: function (b) {
            return !!bridge['_efx_input_button_is_pressed'](__buttonId(b));
        },
        isReleased: function (b) {
            return !!bridge['_efx_input_button_is_released'](__buttonId(b));
        },
        onDown: makeInputRegister(st.mouseDown),
        onUp: makeInputRegister(st.mouseUp),
        onMove: makeInputRegister(st.mouseMove),
        onWheel: makeInputRegister(st.mouseWheel),
    };
    /* x, y, dx, dy, wheel dx, wheel dy, width, height, dpi (one batched read) */
    function __inputState() {
        var buf = __efxScratch();
        bridge['_efx_bridge_input_state'](buf);
        return Array.prototype.slice.call(HEAPF64, buf >> 3, (buf >> 3) + 9);
    }
    function stateGetter(a, b) {
        return {
            get: b === undefined
                ? function () { return __inputState()[a]; }
                : function () { var s = __inputState(); return [s[a], s[b]]; },
        };
    }
    Object.defineProperty(api.mouse, 'position', stateGetter(0, 1));
    Object.defineProperty(api.mouse, 'x', stateGetter(0));
    Object.defineProperty(api.mouse, 'y', stateGetter(1));
    Object.defineProperty(api.mouse, 'delta', stateGetter(2, 3));
    Object.defineProperty(api.mouse, 'wheel', stateGetter(4, 5));
    api.window = {};
    Object.defineProperty(api.window, 'size', stateGetter(6, 7));
    Object.defineProperty(api.window, 'width', stateGetter(6));
    Object.defineProperty(api.window, 'height', stateGetter(7));
    Object.defineProperty(api.window, 'dpiScale', stateGetter(8));

    /* F13 gamepad namespace (desktop parity: same C core, same errors) */
    api.gamepad = {
        get: function (index) {
            if (typeof index !== 'number') {
                throw new TypeError('gamepad.get requires an index');
            }
            var i = index | 0;
            if (!bridge['_efx_input_gamepad_connected'](i)) {
                return null;
            }
            return __efxGamepadView(i);
        },
        onConnect: makeInputRegister(st.gamepadConnect),
        onDisconnect: makeInputRegister(st.gamepadDisconnect),
    };
    Object.defineProperty(api.gamepad, 'count', {
        get: function () { return bridge['_efx_input_gamepad_count'](); },
    });

    /* ---------------------------------------------- F12 physics */

    var physBodies = new Map();
    var physCharacters = new Map();

