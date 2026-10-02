/*
 * F6a: --root overrides the script-directory resource root.
 */
if (efx.io.loadText('hello.txt') !== 'hello efx\n') {
    throw new Error('root override load failed');
}
efx.log('s-6a-root-ok');
