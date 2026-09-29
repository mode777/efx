// F13 gamepad API: portable semantics. Synthetic injection is covered by the
// C-level harness (efx_api_tests gamepad_js), so this script exercises the
// default state, the full validation matrix, unsubscribe idempotence, and the
// absence of resource types — identically on the desktop runtime and the web
// bridge.
function kind(fn) {
    try {
        fn();
        return 'none';
    } catch (e) {
        if (e instanceof TypeError) return 'TypeError';
        if (e instanceof RangeError) return 'RangeError';
        return 'Error';
    }
}

// default state: no pads connected, no slot view
if (efx.gamepad.count !== 0) throw new Error('default count');
if (efx.gamepad.get(0) !== null) throw new Error('default get 0');
if (efx.gamepad.get(3) !== null) throw new Error('default get 3');

// validation matrix: bad index types and non-function registrations throw
if (kind(() => efx.gamepad.get('x')) !== 'TypeError') throw new Error('non-number index');
if (kind(() => efx.gamepad.onConnect(5)) !== 'TypeError') throw new Error('non-fn connect');
if (kind(() => efx.gamepad.onDisconnect(null)) !== 'TypeError') throw new Error('non-fn disconnect');
if (kind(() => efx.gamepad.onConnect({})) !== 'TypeError') throw new Error('non-fn connect2');
if (kind(() => efx.gamepad.onDisconnect([])) !== 'TypeError') throw new Error('non-fn disconnect2');

// unsubscribe is idempotent; an unsubscribed callback never fires
let fired = 0;
const off = efx.gamepad.onConnect(() => { fired++; });
off();
off();
const off2 = efx.gamepad.onDisconnect(() => { fired++; });
off2();
if (fired !== 0) throw new Error('unsubscribed callback fired');

// no resources were added: the namespace holds no create/destroy
if (typeof efx.gamepad.destroy !== 'undefined' ||
    typeof efx.gamepad.create !== 'undefined') {
    throw new Error('gamepad must add no resources');
}
if (typeof efx.gamepad.get !== 'function' ||
    typeof efx.gamepad.onConnect !== 'function' ||
    typeof efx.gamepad.onDisconnect !== 'function') {
    throw new Error('gamepad surface incomplete');
}

efx.log('s-13-gamepad-ok');
