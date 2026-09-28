/*
 * stb_rect_pack implementation TU (F8a font/text).
 *
 * Isolated from the rest of efx_core so the vendored header's warnings stay
 * in one place. Used to pack the fixed glyph atlas deterministically.
 */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
