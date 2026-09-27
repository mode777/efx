/*
 * F6a: loadText / loadImage / loadTexture resolve against the script's
 * directory (the default resource root in --script mode).
 */
var text = efx.loadText('resource_probe.txt');
if (text !== 'probe-text\n') {
    throw new Error('loadText: ' + JSON.stringify(text));
}

var img = efx.loadImage('resource_probe.png');
if (img.width !== 3 || img.height !== 2) {
    throw new Error('loadImage dims: ' + img.width + 'x' + img.height);
}
img.destroy();

var tex = efx.loadTexture('resource_probe.png');
if (tex.width !== 3 || tex.height !== 2) {
    throw new Error('loadTexture dims: ' + tex.width + 'x' + tex.height);
}
tex.destroy();

efx.log('s-6a-resource-ok');
