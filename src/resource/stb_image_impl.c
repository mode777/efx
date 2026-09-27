/*
 * stb_image implementation TU (F6a resource loader).
 *
 * Isolated from the rest of efx_core so the vendored header's warnings and
 * macros (STB_IMAGE_IMPLEMENTATION, the STBI_NO_* format switches) stay in
 * one place. Only PNG and JPEG are enabled — the formats the resource spec
 * requires — keeping the loader's surface small. The 4-channel request and
 * byte order used by efx_image_decode match efx ImageData.
 */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "resource/stb_image_config.h"
#include "stb_image.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
