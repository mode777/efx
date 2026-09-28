// F6e golden: the same 2x2 image drawn greatly minified, without mipmaps
// (left) and with a mip chain (right). Mipmap minification collapses to the
// 1x1 average level, so the left bilinear gradient becomes a flat colour on
// the right; a smaller minified pair sits along the bottom.
efx.setClearColor([0.02, 0.03, 0.06, 1]);
efx.setCamera2D({ frame: [640, 480] });

const img = efx.createImageData({
    width: 2, height: 2,
    pixels: [
        230, 40, 40, 255,   40, 210, 80, 255,
        40, 80, 230, 255,   240, 220, 60, 255,
    ],
});
const plain = efx.createTexture(img);
const mips = efx.createTexture(img, { mipmaps: true, filter: 'linear' });

function update() {}
function render() {
    efx.drawQuad(40, 60, plain, { size: [240, 240] });
    efx.drawQuad(360, 60, mips, { size: [240, 240] });
    efx.drawQuad(320, 40, efx.whiteTexture, { size: [2, 280], color: [0.8, 0.8, 0.8, 1] });
    efx.drawQuad(40, 340, plain, { size: [128, 128] });
    efx.drawQuad(360, 340, mips, { size: [128, 128] });
}
