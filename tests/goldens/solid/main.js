efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);
function update() {}
function render() {
    efx.graphics.drawQuad(220, 150, efx.graphics.whiteTexture, {
        size: [200, 180],
        color: [0.9, 0.2, 0.1, 1],
        rotation: 30,
        scale: 1.2,
    });
}
