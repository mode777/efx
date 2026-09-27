// Preloaded (`node --require`) before the Node web player. Supplies a host
// asset-root URL (F6a async boot) and asserts on process exit that the
// channel was consumed before the entry script ran.
globalThis.__efx_assets = process.env.EFX_ASSETS_URL;

process.on('exit', () => {
    if (globalThis.__efx_assets !== undefined) {
        console.error('web assets preload FAILED: asset channel was not consumed');
        process.exitCode = 1;
    }
});
