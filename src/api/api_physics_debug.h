#ifndef EFX_API_PHYSICS_DEBUG_H
#define EFX_API_PHYSICS_DEBUG_H

/*
 * physics.debugDraw recorder shared by the desktop binding and the web
 * bridge (script-api-ergonomics). Option-bag validation lives in the shared
 * prelude (ADR 0049); this entry records an unlit translucent hull per live
 * collider into the display list. Returns 0, or -1 on allocation failure
 * (the caller throws).
 */

#include "physics/physics.h"

int efx_physics_debug_draw_recorder(efx_physics_world *w, const float color[4]);

#endif /* EFX_API_PHYSICS_DEBUG_H */
