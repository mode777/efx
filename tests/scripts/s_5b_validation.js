// F5b smoke: post-effect chain and render-scale validation, atomicity, and
// lifecycle (portable — identical on desktop and web; the rendered result
// is covered by the unit/golden suites).
function expectThrow(name, kind, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof kind)) {
            efx.log('FAIL kind ' + name + ': ' + e); efx.quit(2);
        }
    }
}
const TE = TypeError, RE = RangeError;

// setPostEffects: list shape and effect names
efx.setPostEffects(null);
efx.setPostEffects([]);
expectThrow('post-string', TE, () => efx.setPostEffects('nope'));
expectThrow('post-entry-number', TE, () => efx.setPostEffects([1]));
expectThrow('post-unknown-effect', TE, () => efx.setPostEffects([{ effect: 'vortex' }]));
expectThrow('post-missing-effect', TE, () => efx.setPostEffects([{ radius: 2 }]));
expectThrow('post-unknown-field', TE, () => efx.setPostEffects([{ effect: 'blur', frob: 1 }]));
expectThrow('post-radius-type', TE, () => efx.setPostEffects([{ effect: 'blur', radius: 'x' }]));
expectThrow('post-radius-zero', RE, () => efx.setPostEffects([{ effect: 'blur', radius: 0 }]));
expectThrow('post-radius-big', RE, () => efx.setPostEffects([{ effect: 'blur', radius: 65 }]));
expectThrow('post-strength-big', RE, () => efx.setPostEffects([{ effect: 'bloom', strength: 1.5 }]));
expectThrow('post-tint-short', RE, () => efx.setPostEffects([{ effect: 'colorFilter', tint: [1, 1] }]));
expectThrow('post-mix-big', RE, () => efx.setPostEffects([{ effect: 'blur', mix: 2 }]));
expectThrow('post-too-many', RE, () => efx.setPostEffects(new Array(9).fill({ effect: 'blur' })));

// atomicity: a rejected call must leave the previous chain usable
efx.setPostEffects([{ effect: 'blur', radius: 5, mix: 0.25 }]);
expectThrow('post-atomic-unknown', TE, () => efx.setPostEffects([{ effect: 'nope' }]));
expectThrow('post-atomic-range', RE, () => efx.setPostEffects([{ effect: 'blur', radius: 0 }]));

// valid v1 entries, defaults, options, mix
efx.setPostEffects([
    { effect: 'colorFilter' },
    { effect: 'colorFilter', brightness: 1.2, contrast: 0.8, saturation: 0.5, tint: [1, 0.5, 0.25, 1], mix: 0.5 },
    { effect: 'blur', radius: 4 },
    { effect: 'bloom', threshold: 0.6, strength: 0.3 },
]);

// setRenderScale: scale bounds and filter values
expectThrow('scale-string', TE, () => efx.setRenderScale('x'));
expectThrow('scale-zero', RE, () => efx.setRenderScale(0));
expectThrow('scale-negative', RE, () => efx.setRenderScale(-1));
expectThrow('scale-big', RE, () => efx.setRenderScale(2.5));
expectThrow('scale-filter-bad', TE, () => efx.setRenderScale(1, { filter: 'bogus' }));
expectThrow('scale-unknown-field', TE, () => efx.setRenderScale(1, { frob: 1 }));
efx.setRenderScale(0.5, { filter: 'nearest' });
efx.setRenderScale(1.5, { filter: 'linear' });
expectThrow('scale-atomic', RE, () => efx.setRenderScale(0));
efx.setRenderScale(1);

efx.setPostEffects(null);
efx.log('s-5b-validation-ok');
