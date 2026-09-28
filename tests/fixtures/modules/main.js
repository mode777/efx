/* F10 entry module fixture: exercises relative + parent-relative resolution,
 * the deterministic .js fallback, cache identity, circular requires,
 * __esModule interop, star re-export and JSON modules. A failed assertion
 * throws and exits non-zero; success logs the milestone marker. */
'use strict';

var math = require('./lib/math.js');
var math2 = require('./lib/math.js');
if (math !== math2) {
    throw new Error('module cache identity');
}
if (math.add(1, 2) !== 3 || math.value !== 21) {
    throw new Error('math exports');
}

var util = require('shared/util.js'); /* root-relative specifier */
if (util.tag !== 'shared') {
    throw new Error('root-relative specifier');
}

var viaParent = require('./lib/uses_parent.js');
if (viaParent !== util) {
    throw new Error('parent-relative specifier');
}

var fallback = require('./lib/math'); /* deterministic .js fallback */
if (fallback !== math) {
    throw new Error('extension fallback');
}

var config = require('./json/config.json');
if (config.name !== 'config' || config.count !== 3) {
    throw new Error('json module');
}

var cyc = require('./cycle/a.js');
if (cyc.name !== 'a' || cyc.b.name !== 'b' || cyc.b.aName !== 'a' || cyc.done !== true) {
    throw new Error('circular require partial exports');
}

var consumer = require('./interop/consumer.js');
if (consumer.got !== 42) {
    throw new Error('__esModule default interop');
}

var star = require('./interop/star.js');
if (star.named !== 'hello' || star.extra !== 'x') {
    throw new Error('star re-export');
}

efx.log('s-10-modules-ok');
efx.quit(0);
