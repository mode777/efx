/*
 * F6a: --root can name a zip archive; loads resolve inside it.
 */
if (efx.io.loadText('data.txt') !== 'zip-data\n') {
    throw new Error('zip root load failed');
}
efx.log('s-6a-root-zip-ok');
