/* Portable efx.io.loadData case: raw bytes as a Uint8Array copy, identical on
 * the desktop quickjs binding and the web bridge. Runs as a resource-root
 * main.js so both runtimes read the same blob.bin. */
var bytes = efx.io.loadData('blob.bin');
if (!(bytes instanceof Uint8Array)) {
    throw new Error('loadData did not return a Uint8Array');
}
if (bytes.length !== 5) {
    throw new Error('loadData length: ' + bytes.length);
}
var want = [0, 1, 2, 254, 255];
for (var i = 0; i < want.length; i++) {
    if (bytes[i] !== want[i]) {
        throw new Error('loadData byte ' + i + ' = ' + bytes[i]);
    }
}
/* the returned array is a copy: mutating it must not affect the next load */
bytes[0] = 99;
var again = efx.io.loadData('blob.bin');
if (again[0] !== 0) {
    throw new Error('loadData result is not an independent copy');
}
/* a missing path throws a standard Error on every runtime */
var threw = 0;
try {
    efx.io.loadData('nope.bin');
} catch (e) {
    threw = (e instanceof Error) ? 1 : 2;
}
if (threw !== 1) {
    throw new Error('missing loadData not Error (' + threw + ')');
}
efx.log('s-6a-loaddata-ok');
efx.quit(0);
