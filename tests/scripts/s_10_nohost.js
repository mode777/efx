/* F10: no Node built-ins or Node globals leak into module scope. */
'use strict';

var leaked = [];
try { require('fs'); leaked.push('fs'); } catch (e) {}
try { require('path'); leaked.push('path'); } catch (e) {}
try { require('node:fs'); leaked.push('node:fs'); } catch (e) {}
if (typeof globalThis.process !== 'undefined') leaked.push('process');
if (typeof globalThis.Buffer !== 'undefined') leaked.push('Buffer');
if (typeof globalThis.require !== 'undefined') leaked.push('require-global');
if (leaked.length > 0) {
    efx.log('module-host-leak:' + leaked.join(','));
    efx.quit(1);
}
efx.log('s-10-nohost-ok');
efx.quit(0);
