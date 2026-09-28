/*
 * stb_truetype implementation TU (F8a font/text).
 *
 * Isolated from the rest of efx_core so the vendored header's warnings and
 * macros stay in one place, like the stb_image TU. stb_truetype provides
 * font parsing, glyph metrics/kerning and antialiased glyph rasterization.
 */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include <math.h>
#include "stb_truetype.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
