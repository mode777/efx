// Text Showcase — an F8a tour. A baked TrueType atlas with outline/shadow
// draws a title, a wrapped paragraph whose alignment cycles through
// left/center/right/justify, and a live typing area: type to append, Enter
// for a new line, Backspace to delete, Tab to cycle the paragraph alignment.
// The font is a CC0 Kenney font shipped in the sample's asset pack.

efx.graphics.setClearColor([0.07, 0.08, 0.12, 1]);
efx.graphics.setCamera2D({ frame: [640, 480] });

const titleFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), {
    size: 44,
    outline: { width: 2 },
    shadow: { blur: 3, offset: [2, 2] },
});
const bodyFont = efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), { size: 24 });

const ALIGNS = ['left', 'center', 'right', 'justify'];
let alignIndex = 0;
let typed = '';
const PLACEHOLDER = 'Type here...';

efx.keyboard.onChar(function (e) {
    if (typed.length < 160) {
        typed += e.char;
    }
});
efx.keyboard.onDown(function (e) {
    if (e.key === 'backspace') {
        typed = typed.slice(0, -1);
    } else if (e.key === 'enter') {
        typed += '\n';
    } else if (e.key === 'tab') {
        alignIndex = (alignIndex + 1) % ALIGNS.length;
    }
});

const paragraph =
    'The quick brown fox jumps over the lazy dog while the engine wraps, ' +
    'centers and justifies each line. Tab cycles the alignment.';

efx.registerRenderHook(function () {
    efx.graphics.drawText('Text Showcase', titleFont, 320, 20, {
        align: 'center',
        color: [1, 0.85, 0.2, 1],
        outlineColor: [0.95, 0.25, 0.05, 1],
        shadowColor: [0, 0, 0, 0.85],
    });
    efx.graphics.drawText('BAKED OUTLINE + SHADOW', titleFont, 320, 86, {
        align: 'center',
        color: [0.85, 0.92, 1, 1],
        outlineColor: [0.1, 0.35, 0.8, 1],
        shadowColor: [0, 0, 0, 0.85],
    });
    efx.graphics.drawText(paragraph, bodyFont, 40, 168, {
        width: 560,
        align: ALIGNS[alignIndex],
        color: [0.85, 0.88, 0.95, 1],
    });
    efx.graphics.drawText('alignment: ' + ALIGNS[alignIndex] + '  (Tab to cycle)',
        bodyFont, 40, 292, { color: [0.5, 0.7, 1, 1] });
    efx.graphics.drawText('Wrapped + justified text from one baked atlas.',
        bodyFont, 40, 328, {
            width: 560,
            align: 'justify',
            color: [0.7, 0.85, 0.7, 1],
        });
    efx.graphics.drawText(typed.length ? typed : PLACEHOLDER, bodyFont, 40, 396, {
        width: 560,
        color: typed.length ? efx.color.white : [0.45, 0.48, 0.55, 1],
    });
});
