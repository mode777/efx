// F6e golden: a 256x256 3px checkerboard drawn minified, without mipmaps
// (left) and with a generated mip chain (right). Without mips the minified
// checker beats into moire; with mips the higher levels average it away.
// The bottom row uses a stronger (4x) minification.
efx.graphics.setClearColor([0.02, 0.03, 0.06, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const S = 256;
const px = new Uint8Array(S * S * 4);
for (let y = 0; y < S; y++) {
    for (let x = 0; x < S; x++) {
        const on = (((x / 3) | 0) + ((y / 3) | 0)) & 1;
        const i = (y * S + x) * 4;
        px[i + 0] = on ? 240 : 20;
        px[i + 1] = on ? 240 : 60;
        px[i + 2] = on ? 240 : 200;
        px[i + 3] = 255;
    }
}
const img = efx.graphics.createImageData(S, S, px);
const plain = efx.graphics.createTexture(img, { filter: 'linear' });
const mips = efx.graphics.createTexture(img, { filter: 'linear', mipmaps: true });

function update() {}
function render() {
    efx.graphics.drawQuad(plain, 60, 70, { size: [128, 128] });
    efx.graphics.drawQuad(mips, 400, 70, { size: [128, 128] });
    efx.graphics.drawQuad(efx.graphics.whiteTexture, 320, 40, { size: [2, 200], color: [0.8, 0.8, 0.8, 1] });
    efx.graphics.drawQuad(plain, 60, 300, { size: [64, 64] });
    efx.graphics.drawQuad(mips, 400, 300, { size: [64, 64] });
}
