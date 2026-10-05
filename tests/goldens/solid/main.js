efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);
function update() {}
function render() {
    efx.graphics.drawQuad(efx.graphics.whiteTexture, 220, 150, {
        size: [200, 180],
        color: [0.9, 0.2, 0.1, 1],
        rotation: 30,
        scale: 1.2,
    });
}
