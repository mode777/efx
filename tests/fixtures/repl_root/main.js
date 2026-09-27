/*
 * F6d piped-REPL fixture: a root whose entry script sets up scene state
 * without quitting, so the console starts with the scene already built.
 */
globalThis.replReady = true;
efx.log('repl-entry-ok');

var frames = 0;
efx.registerUpdateHook(function () {
    frames++;
});
