efx.graphics.setClearColor([0, 0, 0, 1]);
const tex = efx.graphics.createTexture(
    efx.graphics.createImageData({ width: 2, height: 2, pixels: [200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255] }));
function update() {}
function render() {
    efx.graphics.setBlendMode('additive');
    efx.graphics.drawQuad(140, 140, tex, { size: [200, 200], color: [0.15, 0.35, 0.15, 1] });
    efx.graphics.setBlendMode('alpha');
    efx.graphics.drawQuad(260, 180, tex, { size: [200, 200], color: [1, 1, 1, 0.5] });
    efx.graphics.setBlendMode('subtractive');
    efx.graphics.drawQuad(340, 220, tex, { size: [180, 180], color: [0.4, 0.1, 0.4, 1] });
    efx.graphics.setBlendMode('alpha');
}
