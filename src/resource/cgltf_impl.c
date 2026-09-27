/*
 * Isolated cgltf implementation translation unit (F6b). Compiled with
 * warnings relaxed (the vendored-include treatment, ADR 0006) so cgltf's
 * diagnostics never trip the engine's -Werror; the rest of the engine sees
 * only cgltf's declarations via cgltf.h.
 */
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
