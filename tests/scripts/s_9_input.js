// F9 input API: portable semantics. Synthetic injection is covered by the
// C-level harness, so this script exercises the default state, the full
// validation matrix, unsubscribe idempotence, and read-only properties —
// identically on the desktop runtime and the web bridge.
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

// default state: nothing down, pointer at the origin, unit dpi
if (efx.keyboard.isDown('space') !== false) throw new Error('default down');
if (efx.keyboard.isPressed('space') !== false) throw new Error('default pressed');
if (efx.keyboard.isReleased('space') !== false) throw new Error('default released');
if (efx.mouse.isDown('left') !== false) throw new Error('default mouse down');
if (efx.mouse.isPressed('right') !== false) throw new Error('default mouse pressed');
if (efx.mouse.isReleased('middle') !== false) throw new Error('default mouse released');
if (efx.mouse.position[0] !== 0 || efx.mouse.position[1] !== 0) throw new Error('default pointer');
if (efx.mouse.delta[0] !== 0 || efx.mouse.delta[1] !== 0) throw new Error('default delta');
if (efx.mouse.wheel[0] !== 0 || efx.mouse.wheel[1] !== 0) throw new Error('default wheel');
if (efx.window.width !== 0 || efx.window.height !== 0) throw new Error('default window');
if (efx.window.size[0] !== 0 || efx.window.size[1] !== 0) throw new Error('default window size');
if (efx.window.dpiScale !== 1) throw new Error('default dpi');
if (efx.mouse.position.length !== 2 || efx.mouse.delta.length !== 2 ||
    efx.mouse.wheel.length !== 2 || efx.window.size.length !== 2) {
    throw new Error('pair shape');
}

// validation matrix: unknown names and non-function registrations throw
if (kind(() => efx.keyboard.isDown('notakey')) !== 'TypeError') throw new Error('unknown key');
if (kind(() => efx.keyboard.isPressed('notakey')) !== 'TypeError') throw new Error('unknown key p');
if (kind(() => efx.keyboard.isReleased('notakey')) !== 'TypeError') throw new Error('unknown key r');
if (kind(() => efx.mouse.isDown('side')) !== 'TypeError') throw new Error('unknown button');
if (kind(() => efx.keyboard.isDown(5)) !== 'TypeError') throw new Error('non-string key');
if (kind(() => efx.mouse.isDown(null)) !== 'TypeError') throw new Error('non-string button');
if (kind(() => efx.keyboard.onDown(5)) !== 'TypeError') throw new Error('non-fn down');
if (kind(() => efx.keyboard.onUp('x')) !== 'TypeError') throw new Error('non-fn up');
if (kind(() => efx.keyboard.onChar(0)) !== 'TypeError') throw new Error('non-fn char');
if (kind(() => efx.mouse.onDown({})) !== 'TypeError') throw new Error('non-fn mdown');
if (kind(() => efx.mouse.onUp([])) !== 'TypeError') throw new Error('non-fn mup');
if (kind(() => efx.mouse.onMove(1)) !== 'TypeError') throw new Error('non-fn mmove');
if (kind(() => efx.mouse.onWheel('x')) !== 'TypeError') throw new Error('non-fn mwheel');

// unsubscribe is idempotent; an unsubscribed callback never fires
let fired = 0;
const off = efx.keyboard.onDown(() => { fired++; });
off();
off();
const off2 = efx.mouse.onWheel(() => { fired++; });
off2();

// read-only properties ignore assignment
efx.mouse.x = 999;
efx.window.width = 999;
if (efx.mouse.x !== 0) throw new Error('mouse.x must be read-only');
if (efx.window.width !== 0) throw new Error('window.width must be read-only');

// no resources were added: the namespaces hold no destroy()
if (typeof efx.keyboard.destroy !== 'undefined' ||
    typeof efx.mouse.destroy !== 'undefined' ||
    typeof efx.window.destroy !== 'undefined') {
    throw new Error('input namespaces must add no resources');
}

efx.log('s-9-input-ok');
