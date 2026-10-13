// A finite size component <= 0 records nothing (and throws nothing): this
// frame must be pixel-identical to one with only the two valid draws below.
efx.graphics.setClearColor([0.05, 0.05, 0.08, 1]);

function update() {}

function render() {
    const tex = efx.graphics.whiteTexture;
    efx.graphics.drawQuad(tex, 40, 40, { size: [80, 60], color: [1, 0.4, 0.1, 1] });
    // degenerate draws between valid ones: zero width, zero height, negative
    efx.graphics.drawQuad(tex, 160, 40, { size: [0, 60], color: [0.2, 1, 0.3, 1] });
    efx.graphics.drawQuad(tex, 160, 40, { size: [80, 0], color: [0.2, 1, 0.3, 1] });
    efx.graphics.drawQuad(tex, 160, 40, { size: [-40, 60], color: [0.2, 1, 0.3, 1] });
    efx.graphics.drawQuad(tex, 280, 40, { size: [80, 60], color: [0.1, 0.5, 1, 1] });
}
