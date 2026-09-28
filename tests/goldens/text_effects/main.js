/*
 * F8a golden: baked outline + shadow effects drawn as ordered layers.
 */
efx.setClearColor([0.10, 0.10, 0.14, 1]);
efx.setCamera2D({ frame: [640, 480] });

var font = efx.createFont(efx.loadFontData('font.ttf'), {
    size: 56,
    filter: 'nearest',
    outline: { width: 3 },
    shadow: { blur: 4, offset: [4, 4] },
});

efx.registerRenderHook(function () {
    efx.drawText('Outline', font, 40, 50, {
        color: [1, 0.85, 0.2, 1],
        outlineColor: [0.15, 0.06, 0.0, 1],
        shadowColor: [0, 0, 0, 0.6],
    });
    efx.drawText('Shadow', font, 40, 180, {
        color: [0.8, 0.9, 1, 1],
        outlineColor: [0.05, 0.05, 0.12, 1],
        shadowColor: [0, 0, 0, 0.6],
    });
    efx.drawText('glow', font, 320, 320, {
        align: 'center',
        color: [0.9, 0.95, 1, 1],
        outlineColor: [0.1, 0.3, 0.5, 1],
        shadowColor: [0.0, 0.1, 0.2, 0.8],
    });
});
