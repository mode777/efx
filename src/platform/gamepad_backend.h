#ifndef EFX_GAMEPAD_BACKEND_H
#define EFX_GAMEPAD_BACKEND_H

/*
 * F13 platform gamepad backend (design D1/D2): a thin wrapper over the
 * vendored minigamepad snapshot. It is compiled into efx_platform only and
 * never into the pure-C core or the headless test targets; the core sees
 * only engine-owned device descriptors through the registered source.
 */

void efx_gamepad_backend_init(void);
void efx_gamepad_backend_shutdown(void);

#endif
