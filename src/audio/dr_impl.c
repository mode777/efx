/*
 * F14 audio decoders: the single implementation TU for the vendored dr_libs
 * headers (`dr_wav` + `dr_mp3`). Compiled with warnings relaxed, matching the
 * miniz/cgltf/stb implementation TUs. The rest of the engine includes only
 * the declarations from `vendor/dr_libs/`.
 *
 * Both headers are designed to coexist in one TU. The engine uses the
 * memory-backed incremental decode paths (`drwav_init_memory` /
 * `drmp3_init_memory`) so resources are read through the F6a provider.
 */

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
