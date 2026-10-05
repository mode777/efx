efx.graphics.setClearColor([0, 0, 0, 1]);
const tex = efx.graphics.createTexture(
    efx.graphics.createImageData(2, 2, [200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255, 200, 200, 200, 255]));
function update() {}
function render() {
    efx.graphics.setBlendMode('additive');
    efx.graphics.drawQuad(tex, 140, 140, { size: [200, 200], color: [0.15, 0.35, 0.15, 1] });
    efx.graphics.setBlendMode('alpha');
    efx.graphics.drawQuad(tex, 260, 180, { size: [200, 200], color: [1, 1, 1, 0.5] });
    efx.graphics.setBlendMode('subtractive');
    efx.graphics.drawQuad(tex, 340, 220, { size: [180, 180], color: [0.4, 0.1, 0.4, 1] });
    efx.graphics.setBlendMode('alpha');
}
