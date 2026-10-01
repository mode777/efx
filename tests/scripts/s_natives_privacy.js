/* ADR 0049: the prelude's natives object and its validators are unreachable
 * from scripts on both runtimes (js-api requirement "Shared argument
 * handling stays private"). Enumerates `efx` and the global scope and fails
 * on any engine-internal name. Runs unchanged on desktop quickjs and the
 * web binding. */

var INTERNAL_NAMES = [
    'natives',
    '__efxParticleWire', '__efxPsBags', '__efxSnapshotOpts',
    '__efxCheckKnown', '__efxPreludeInstall', '__efxCreateModuleRuntime',
    '__efxModuleResolve', '__efxModuleNormalize', '__efxModuleDirname',
    '__efxSourceRect', '__efxPartVec', '__efxPartRange', '__efxPartEnum',
    '__efxRc',
];

var leaked = [];

function scan(where, obj) {
    var names = Object.getOwnPropertyNames(obj);
    for (var i = 0; i < names.length; i++) {
        for (var j = 0; j < INTERNAL_NAMES.length; j++) {
            if (names[i] === INTERNAL_NAMES[j]) {
                leaked.push(where + '.' + names[i]);
            }
        }
    }
}

scan('efx', efx);
scan('globalThis', globalThis);

if (leaked.length > 0) {
    throw new Error('engine internals reachable from scripts: ' +
        leaked.join(', '));
}

efx.log('s-natives-privacy-ok');
efx.quit(0);
