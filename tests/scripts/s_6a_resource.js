/*
 * F6a: loadText / loadImage resolve against the script's directory (the
 * default resource root in --script mode); a texture is the composed
 * createTexture(loadImage(path), opts?) flow (F6e).
 */
var text = efx.loadText('resource_probe.txt');
if (text !== 'probe-text\n') {
    throw new Error('loadText: ' + JSON.stringify(text));
}

var img = efx.graphics.loadImage('resource_probe.png');
if (img.width !== 3 || img.height !== 2) {
    throw new Error('loadImage dims: ' + img.width + 'x' + img.height);
}
img.destroy();

var tex = efx.graphics.createTexture(efx.graphics.loadImage('resource_probe.png'));
if (tex.width !== 3 || tex.height !== 2) {
    throw new Error('composed tex dims: ' + tex.width + 'x' + tex.height);
}
tex.destroy();

var mip = efx.graphics.createTexture(efx.graphics.loadImage('resource_probe.png'),
                            { wrap: 'clamp', mipmaps: true });
if (mip.width !== 3 || mip.height !== 2) {
    throw new Error('mipmapped tex dims: ' + mip.width + 'x' + mip.height);
}
mip.destroy();

if (typeof efx.loadTexture !== 'undefined') {
    throw new Error('loadTexture should not exist');
}

efx.log('s-6a-resource-ok');
