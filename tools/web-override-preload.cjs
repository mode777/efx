// Preloaded (`node --require`) before the Node web player. Sets the host
// entry source, then asserts on process exit that it ran and that the host
// channel was consumed. See tools/test_web_override.mjs.
const source = [
    "efx.log('OVERRIDE-RAN');",
    'globalThis.__efx_override_ran = true;',
    'efx.quit(0);',
].join('\n');

globalThis.__efx_main_js = source;

process.on('exit', () => {
    const fails = [];
    if (globalThis.__efx_override_ran !== true) {
        fails.push('host-provided source did not run');
    }
    if (globalThis.__efx_main_js !== undefined) {
        fails.push('host channel was not consumed before evaluation');
    }
    if (fails.length) {
        console.error('web override preload FAILED: ' + fails.join('; '));
        process.exitCode = 1;
    } else {
        console.log('web override preload PASSED');
    }
});
