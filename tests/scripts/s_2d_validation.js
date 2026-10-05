// F2 smoke: argument validation of the 2D API (headless — throws must be
// standard ES6 errors; draw calls that need a GPU surface are covered by
// windowed/golden tests)
function expectThrow(name, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof TypeError) && !(e instanceof RangeError)) {
            efx.log('FAIL type ' + name + ': ' + e); efx.quit(2);
        }
    }
}
expectThrow('clearColor-wrongtype', () => efx.graphics.setClearColor('red'));
expectThrow('clearColor-short', () => efx.graphics.setClearColor([1, 2, 3]));
expectThrow('camera-noargs', () => efx.graphics.setCamera2D());
expectThrow('camera-zoom', () => efx.graphics.setCamera2D({ x: 0, y: 0, zoom: 0 }));
expectThrow('camera-frame', () => efx.graphics.setCamera2D({ frame: [0, 0] }));
expectThrow('camera-frame3', () => efx.graphics.setCamera2D({ frame: [10, 10, 10] }));
expectThrow('blend-unknown', () => efx.graphics.setBlendMode('nope'));
expectThrow('blend-missing', () => efx.graphics.setBlendMode());
expectThrow('img-short', () => efx.graphics.createImageData(2, 2, [1, 2, 3]));
expectThrow('img-fmt', () => efx.graphics.createImageData(1, 1, [0, 0, 0, 0], { format: 'bgr' }));
expectThrow('img-unknown', () => efx.graphics.createImageData(1, 1, [0, 0, 0, 0], { pixles: 1 }));
expectThrow('img-size', () => efx.graphics.createImageData(0, 8, []));

// drawQuad(x, y, texture, opts?): texture required live; size/origin
// validation; created textures and the engine whiteTexture both work
// headless as CPU-only resources (ADR 0052)
const img = efx.graphics.createImageData(8, 4, new Uint8Array(8 * 4 * 4));
const tex = efx.graphics.createTexture(img);
const white = efx.graphics.whiteTexture;
if (white.width !== 1 || white.height !== 1) { efx.log('FAIL white dims'); efx.quit(4); }
efx.graphics.drawQuad(white, 0, 0, { size: [4, 4] });
if (tex.width !== 8 || tex.height !== 4) { efx.log('FAIL texture size getters'); efx.quit(3); }
expectThrow('quad-few', () => efx.graphics.drawQuad(tex, 0));
expectThrow('quad-nontexture', () => efx.graphics.drawQuad({}, 0, 0));
expectThrow('quad-size-zero', () => efx.graphics.drawQuad(tex, 0, 0, { size: [0, 10] }));
expectThrow('quad-size-short', () => efx.graphics.drawQuad(tex, 0, 0, { size: [10] }));
expectThrow('quad-size-type', () => efx.graphics.drawQuad(tex, 0, 0, { size: 'big' }));
expectThrow('quad-size-nan', () => efx.graphics.drawQuad(tex, 0, 0, { size: [NaN, 1] }));
expectThrow('quad-origin-nan', () => efx.graphics.drawQuad(tex, 0, 0, { origin: [NaN, 0] }));
expectThrow('quad-origin-type', () => efx.graphics.drawQuad(tex, 0, 0, { origin: 'center' }));
expectThrow('quad-src-zero', () => efx.graphics.drawQuad(tex, 0, 0, { sourceRect: { x: 0, y: 0, w: 0, h: 2 } }));
expectThrow('quad-src-oob', () => efx.graphics.drawQuad(tex, 0, 0, { sourceRect: { x: 0, y: 0, w: 9, h: 2 } }));
expectThrow('quad-unknown-opt', () => efx.graphics.drawQuad(tex, 0, 0, { colour: [1, 1, 1, 1] }));
tex.destroy();
expectThrow('quad-destroyed-texture', () => efx.graphics.drawQuad(tex, 0, 0));
expectThrow('getter-destroyed-texture', () => tex.width);
expectThrow('getter-destroyed-texture-h', () => tex.height);
efx.log('2d-validation-ok');
efx.quit(0);
