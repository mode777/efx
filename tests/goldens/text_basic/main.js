/*
 * F8a golden: filled text — a heading, the three horizontal alignments, a
 * wrapped paragraph, and a justified line. Exercises font baking, layout,
 * wrapping, and alignment end to end.
 */
efx.setClearColor([0.08, 0.09, 0.12, 1]);
efx.setCamera2D({ frame: [640, 480] });

var font = efx.createFont(efx.loadFontData('font.ttf'), { size: 32, filter: 'nearest' });

efx.registerRenderHook(function () {
    efx.drawText('EmotionFX', font, 20, 16, { color: [1, 1, 1, 1] });
    efx.drawText('left aligned', font, 20, 80, { color: [0.9, 0.3, 0.2, 1] });
    efx.drawText('center aligned', font, 320, 130,
        { align: 'center', color: [0.3, 0.9, 0.4, 1] });
    efx.drawText('right aligned', font, 620, 180,
        { align: 'right', color: [0.35, 0.55, 1, 1] });
    efx.drawText('The quick brown fox jumps over the lazy dog.',
        font, 20, 230, { width: 300, color: [0.9, 0.9, 0.9, 1] });
    efx.drawText('justified text fills the whole line width here', font, 20,
        360, { width: 360, align: 'justify', color: [0.85, 0.85, 0.55, 1] });
});
