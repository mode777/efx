/*
 * F8a text smoke (portable across the desktop and web bindings): load a TTF,
 * bake a fixed atlas, measure/wrap, draw, bake effects, and check the option
 * errors. Prints s-8a-text-ok on success.
 */
var fd = efx.loadFontData('font.ttf');
var font = efx.createFont(fd, { size: 24 });
if (font.size !== 24) throw new Error('size');
if (!(font.lineHeight > 0)) throw new Error('lineHeight');
if (!(font.ascent > 0) || !(font.descent < 0)) throw new Error('vmetrics');

var m = efx.measureText('hello world', font, { width: 60 });
if (m.lines < 2) throw new Error('wrap');
var b = efx.drawText('hello', font, 8, 8, { color: [1, 1, 1, 1] });
if (b.lines !== 1) throw new Error('draw');

var fx = efx.createFont(fd, {
    size: 20,
    outline: { width: 2 },
    shadow: { blur: 2, offset: [1, 1] },
});
efx.drawText('Hi', fx, 0, 0, {
    align: 'center',
    outlineColor: [0, 0, 0, 1],
    shadowColor: [0, 0, 0, 1],
});

function boom(fn) {
    try { fn(); } catch (e) {
        return e && e.constructor ? e.constructor.name : 'Error';
    }
    return 'none';
}
if (boom(function () { efx.createFont(fd, {}); }) !== 'TypeError') {
    throw new Error('missing size');
}
if (boom(function () { efx.createFont(fd, { size: 0 }); }) !== 'RangeError') {
    throw new Error('size 0');
}
if (boom(function () { efx.createFont(fd, { size: 16, nope: 1 }); }) !== 'TypeError') {
    throw new Error('unknown option');
}
if (boom(function () { efx.drawText('x', font, 0, 0, { align: 'justify' }); }) !== 'TypeError') {
    throw new Error('justify without width');
}

font.destroy();
fx.destroy();
fd.destroy();
efx.log('s-8a-text-ok');
efx.quit(0);
