#ifndef EFX_PLAYER_H
#define EFX_PLAYER_H

#include "runtime/runtime.h"

int efx_player_main(int argc, char **argv);
int efx_player_frame(void *rt, double dt);

/* ADR 0007 exit code: 1 on error, the efx.quit code, otherwise 0 */
int efx_player_exit_code(efx_runtime *rt);
/* run `code` (freed here) as the main.js entry: 0 continues into the frame
 * loop; 1 stops with *exit_code set */
int efx_player_run_entry(efx_runtime *rt, char *code, int *exit_code);

#endif
