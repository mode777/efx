cmake_minimum_required(VERSION 3.21)
project(emotionfx C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

if(EMSCRIPTEN)
    # EM_ASM requires GNU statement expressions
    set(CMAKE_C_EXTENSIONS ON)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT EMSCRIPTEN)
    # ADR 0022: quickjs never ships to the browser — the web player is the
    # C core + the page's native JS engine through the src/web/ bridge.
    set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/quickjs-ng EXCLUDE_FROM_ALL)
endif()

# Vendored single-file/single-header C libraries are built as their own
# targets with relaxed warnings so their diagnostics never trip efx_core's
# -Werror. `POSIX_DEFINITIONS` apply on non-MSVC, `MSVC_DEFINITIONS` on MSVC.
function(efx_add_vendor_library name)
    cmake_parse_arguments(VL "" ""
        "SOURCES;INCLUDES;MSVC_DEFINITIONS;POSIX_DEFINITIONS" ${ARGN})
    add_library(${name} STATIC ${VL_SOURCES})
    target_include_directories(${name} PUBLIC ${VL_INCLUDES})
    if(MSVC)
        target_compile_options(${name} PRIVATE /W3)
        if(VL_MSVC_DEFINITIONS)
            target_compile_definitions(${name} PRIVATE ${VL_MSVC_DEFINITIONS})
        endif()
    else()
        target_compile_options(${name} PRIVATE -w)
        if(VL_POSIX_DEFINITIONS)
            target_compile_definitions(${name} PRIVATE ${VL_POSIX_DEFINITIONS})
        endif()
    endif()
endfunction()

# F6a zip backend: miniz is a vendored single-file amalgamation.
efx_add_vendor_library(efx_miniz
    SOURCES vendor/miniz/miniz.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniz
    # miniz uses ftello/fseeko (large-file stdio) on POSIX
    POSIX_DEFINITIONS _POSIX_C_SOURCE=200809L)

# F6b glTF parser: cgltf is one header with its implementation compiled in a
# dedicated TU.
efx_add_vendor_library(efx_cgltf
    SOURCES src/resource/cgltf_impl.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cgltf)

# F14 audio decoders: dr_libs (dr_wav + dr_mp3) is a single-header pair with
# its implementation compiled in a dedicated TU.
efx_add_vendor_library(efx_drlibs
    SOURCES src/audio/dr_impl.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dr_libs
    MSVC_DEFINITIONS _CRT_SECURE_NO_WARNINGS)

# Per-module source lists, each spelled out once: efx_core (below) and the
# headless test executables (bottom of this file) are composed from them.
set(EFX_RENDER_CORE_SOURCES
    src/render/render_records.c
    src/render/render_texture.c
    src/render/render_target.c
    src/render/render_post.c
    src/render/render_mesh.c
    src/render/render_particles.c
    src/render/skin.c
)
set(EFX_TEXT_SOURCES
    src/render/text.c
    src/render/stb_truetype_impl.c
    src/render/stb_rect_pack_impl.c
)
set(EFX_INPUT_SOURCES
    src/input/input.c
    src/input/gamepad.c
)
set(EFX_PHYSICS_SOURCES
    src/physics/shape.c
    src/physics/broadphase.c
    src/physics/narrow.c
    src/physics/solver.c
    src/physics/character.c
    src/physics/world.c
)
set(EFX_AUDIO_SOURCES
    src/audio/audio.c
    src/audio/decode.c
)
set(EFX_RESOURCE_SOURCES
    src/resource/image.c
    src/resource/resource.c
    src/resource/gltf.c
    src/resource/stb_image_impl.c
)
# desktop binding + runtime: the desktop efx_core variant appends these; the
# web build gets its binding from the src/web/ bridge instead (ADR 0022)
set(EFX_API_SOURCES
    src/api/api.c
    src/api/api_2d.c
    src/api/api_3d.c
    src/api/api_lighting.c
    src/api/api_target_post.c
    src/api/api_resource.c
    src/api/api_text.c
    src/api/api_particles.c
    src/api/api_input.c
    src/api/api_physics.c
    src/api/api_audio.c
    src/runtime/runtime.c
)

if(EMSCRIPTEN)
    # P10: the bridge fragments are linked directly into each web executable
    # rather than the static archive. The wasm linker drops archive members
    # with no incoming C reference, and the post-js glue reaches these
    # EMSCRIPTEN_KEEPALIVE functions by name, so a single bridge object used
    # to be pulled in only because efx_web_main lived beside them.
    set(EFX_WEB_BRIDGE
        src/web/bridge_core.c
        src/web/bridge_render2d.c
        src/web/bridge_resource.c
        src/web/bridge_text.c
        src/web/bridge_target_post.c
        src/web/bridge_particles.c
        src/web/bridge_render3d.c
        src/web/bridge_input.c
        src/web/bridge_audio.c
        src/web/bridge_physics.c)
endif()
add_library(efx_core STATIC
    ${EFX_RENDER_CORE_SOURCES}
    ${EFX_TEXT_SOURCES}
    ${EFX_INPUT_SOURCES}
    ${EFX_PHYSICS_SOURCES}
    ${EFX_AUDIO_SOURCES}
    ${EFX_RESOURCE_SOURCES}
)
if(NOT EMSCRIPTEN)
    target_sources(efx_core PRIVATE ${EFX_API_SOURCES})
endif()
target_include_directories(efx_core PUBLIC src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb)
target_link_libraries(efx_core PUBLIC efx_miniz efx_cgltf efx_drlibs)
if(NOT EMSCRIPTEN)
    target_link_libraries(efx_core PUBLIC qjs)
endif()
if(UNIX)
    target_link_libraries(efx_core PUBLIC m)
endif()
if(MSVC)
    target_compile_options(efx_core PRIVATE /W4 /WX)
else()
    target_compile_options(efx_core PRIVATE -Wall -Wextra -Werror)
endif()
if(NOT MSVC)
    target_compile_definitions(efx_core PRIVATE _POSIX_C_SOURCE=200809L)
endif()

add_library(efx_math STATIC
    src/math/efx_math.cpp
)
target_include_directories(efx_math PUBLIC src)
target_include_directories(efx_math SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
if(MSVC)
    # GLM headers are not /W4-clean; the wrapper TU itself is
    target_compile_options(efx_math PRIVATE /W3)
else()
    target_compile_options(efx_math PRIVATE -Wall -Wextra -Werror)
endif()

# headless developer builds: unit-test targets only — no sokol platform,
# player, or golden tests (the display-required targets need X11/GL on
# UNIX); the full build matrix is the verification server's and CI's job
option(EFX_HEADLESS "Build headless unit-test targets only" OFF)

if(NOT EFX_HEADLESS)
add_library(efx_platform STATIC
    src/platform/platform.c
    src/platform/pipeline.c
    src/platform/capture.c
    src/platform/gamepad_backend.c
    src/platform/audio_backend.c
)
target_include_directories(efx_platform PUBLIC src ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sokol
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/vendor/minigamepad)
target_link_libraries(efx_platform PUBLIC efx_math)
if(APPLE)
    # minigamepad's macOS backend talks to the IOKit HID manager;
    # sokol_audio's CoreAudio backend needs AudioToolbox
    target_link_libraries(efx_platform PRIVATE "-framework IOKit" "-framework CoreFoundation" "-framework AudioToolbox")
endif()
if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    # sokol_audio's Linux backend is ALSA (design D1)
    target_link_libraries(efx_platform PRIVATE asound pthread dl)
endif()
if(MSVC)
    target_compile_options(efx_platform PRIVATE /W4)
elseif(NOT EMSCRIPTEN)
    target_compile_options(efx_platform PRIVATE -Wall -Wextra)
endif()
if(UNIX AND NOT APPLE)
    target_compile_definitions(efx_platform PRIVATE _POSIX_C_SOURCE=200809L)
endif()

if(EMSCRIPTEN)
    add_executable(player src/web/web_main.c ${EFX_WEB_BRIDGE})
else()
    add_executable(player
        src/main.c
        src/player/player.c
        src/player/repl.c
    )
endif()
target_link_libraries(player PRIVATE efx_core efx_platform)

if(EMSCRIPTEN)
    # P10: the web binding is split into ordered fragments that emcc appends
    # as consecutive --post-js files; concatenated in order they reproduce
    # the former single entry.js exactly.
    set(EFX_WEB_JS
        src/web/js/core.js
        src/web/js/render2d.js
        src/web/js/resource.js
        src/web/js/text.js
        src/web/js/target_post.js
        src/web/js/particles.js
        src/web/js/render3d.js
        src/web/js/input.js
        src/web/js/physics.js
        src/web/js/audio.js
        src/web/js/boot.js)
    set(EFX_WEB_POST_JS "")
    set(EFX_WEB_JS_PATHS "")
    foreach(js ${EFX_WEB_JS})
        list(APPEND EFX_WEB_POST_JS "--post-js=${CMAKE_CURRENT_SOURCE_DIR}/${js}")
        list(APPEND EFX_WEB_JS_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/${js}")
    endforeach()

    # core functions the web JS calls directly instead of through a
    # passthrough bridge export (the core stays free of Emscripten macros,
    # ADR 0003); tools/check_exports.mjs reads this list
    set(EFX_WEB_CORE_EXPORTS
        _efx_input_key_id _efx_input_button_id
        _efx_input_key_name _efx_input_button_name
        _efx_input_key_is_down _efx_input_key_is_pressed
        _efx_input_key_is_released _efx_input_button_is_down
        _efx_input_button_is_pressed _efx_input_button_is_released
        _efx_input_event_count _efx_input_clear_events
        _efx_input_gamepad_count _efx_input_gamepad_connected
        _efx_input_gamepad_mapped _efx_input_gamepad_button_id
        _efx_input_gamepad_axis_id _efx_input_gamepad_button_is_down
        _efx_input_gamepad_button_is_pressed
        _efx_input_gamepad_button_is_released _efx_input_gamepad_axis
        _efx_input_gamepad_raw_button _efx_input_gamepad_raw_axis
        _efx_input_gamepad_connect_count _efx_input_gamepad_connect_at
        _efx_input_gamepad_disconnect_count _efx_input_gamepad_disconnect_at
        _efx_audio_master_volume _efx_audio_set_master_volume
        _efx_audio_request_resume
        _efx_render_set_blend _efx_render_end_target
        _efx_render_set_render_scale)
    list(JOIN EFX_WEB_CORE_EXPORTS "," EFX_WEB_CORE_EXPORTS_CSV)

    set(EFX_WEB_COMMON
        -sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=2MB
        -sEXPORTED_FUNCTIONS=_main,_malloc,_free,${EFX_WEB_CORE_EXPORTS_CSV}
        # the web bridge's JS (js/*.js/boot_pre.js) uses the FS API at
        # runtime for resource roots; keep it linked even when no C code
        # touches files (nothing else forces FS inclusion on the node player)
        -sFORCE_FILESYSTEM=1
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js"
        ${EFX_WEB_POST_JS})

    # Node-runnable web player (host-engine smoke/comparison harness; the
    # desktop-style --script mode is a desktop-only run mode)
    set_target_properties(player PROPERTIES
        SUFFIX ".js"
        LINK_DEPENDS "${EFX_WEB_JS_PATHS};${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js")
    target_link_options(player PRIVATE
        ${EFX_WEB_COMMON}
        -sMAX_WEBGL_VERSION=2
        -sNODERAWFS=1 -sENVIRONMENT=node)

    # Raw web player + downloadable bundle. Its preloaded root
    # (examples/browser) is only the standalone fallback demo; the public
    # sample gallery is a separate site under gallery/ (ADR 0030) and feeds
    # the engine a host-provided entry script at boot.
    add_executable(player_web src/web/web_main.c ${EFX_WEB_BRIDGE})
    target_link_libraries(player_web PRIVATE efx_core efx_platform)
    set_target_properties(player_web PROPERTIES
        SUFFIX ".html"
        LINK_DEPENDS "${EFX_WEB_JS_PATHS};${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js;${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web_pre.js;${CMAKE_CURRENT_SOURCE_DIR}/examples/browser/main.js")
    target_link_options(player_web PRIVATE
        ${EFX_WEB_COMMON}
        -sMAX_WEBGL_VERSION=2 -sENVIRONMENT=web
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/examples/browser@/examples/browser"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web_pre.js")

    # browser capture build for the Emscripten golden-image job (ADR 0020):
    # goldens scenes are preloaded to /goldens/<scene>, harness fixture roots
    # to /fixtures/<name>; captures are exported via canvas.toDataURL and
    # read back by tools/run_web_goldens.mjs / tools/run_web_harness.mjs
    if(EFX_BUILD_GOLDEN_TESTS)
        add_executable(player_web_golden src/web/web_main.c ${EFX_WEB_BRIDGE})
        target_link_libraries(player_web_golden PRIVATE efx_core efx_platform)
        target_compile_definitions(player_web_golden PRIVATE EFX_WEB_GOLDEN=1)
        set_target_properties(player_web_golden PROPERTIES
            SUFFIX ".js"
            LINK_DEPENDS "${EFX_WEB_JS_PATHS};${CMAKE_CURRENT_SOURCE_DIR}/src/web/boot_pre.js")
        target_link_options(player_web_golden PRIVATE
            ${EFX_WEB_COMMON}
            -sMAX_WEBGL_VERSION=2 -sENVIRONMENT=web -sSTACK_SIZE=4MB
            "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens@/goldens"
            "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures@/fixtures")
    endif()
elseif(APPLE)
    enable_language(OBJC)
    set_source_files_properties(src/platform/platform.c src/platform/pipeline.c src/platform/capture.c PROPERTIES LANGUAGE OBJC)
    target_link_libraries(efx_platform PRIVATE
        "-framework Cocoa" "-framework QuartzCore"
        "-framework Metal" "-framework MetalKit")
elseif(WIN32)
    target_compile_definitions(efx_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(player PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(efx_platform PRIVATE d3d11 dxgi uuid user32 gdi32 shell32)
elseif(UNIX)
    # the REPL polls stdin with poll(2)/read(2) (design D1); strict C11
    # hides them without the POSIX feature macro
    target_compile_definitions(player PRIVATE _POSIX_C_SOURCE=200809L)
    find_path(EFX_X11_INCLUDE_DIR X11/Xlib.h REQUIRED)
    find_library(EFX_X11_LIBRARY X11 REQUIRED)
    find_library(EFX_XI_LIBRARY Xi REQUIRED)
    find_library(EFX_XCURSOR_LIBRARY Xcursor REQUIRED)
    find_path(EFX_GL_INCLUDE_DIR GL/gl.h REQUIRED)
    find_library(EFX_GL_LIBRARY GL REQUIRED)
    target_include_directories(efx_platform PRIVATE
        ${EFX_X11_INCLUDE_DIR} ${EFX_GL_INCLUDE_DIR})
    target_link_libraries(efx_platform PRIVATE
        ${EFX_X11_LIBRARY} ${EFX_XI_LIBRARY} ${EFX_XCURSOR_LIBRARY}
        ${EFX_GL_LIBRARY} m)
endif()
endif() # NOT EFX_HEADLESS

option(EFX_BUILD_DEV_HARNESS "Build the temporary dev harness (F1 implementation aid)" OFF)

# regenerate shaders/quad.h + shaders/mesh.h when the pinned sokol-shdc
# binary is provided:
#   cmake -B build -DEFX_SHDC=/path/to/sokol-shdc ...
set(EFX_SHDC "" CACHE FILEPATH "Path to the pinned sokol-shdc binary (regenerates shaders/quad.h and shaders/mesh.h)")
if(EFX_SHDC)
    add_custom_target(regen_shaders
        COMMAND ${EFX_SHDC} -i ${CMAKE_CURRENT_SOURCE_DIR}/shaders/quad.glsl
                -o ${CMAKE_CURRENT_SOURCE_DIR}/shaders/quad.h
                --slang glsl410:glsl300es:hlsl4:metal_macos -f sokol_impl
        COMMAND ${EFX_SHDC} -i ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.glsl
                -o ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.h
                --slang glsl410:glsl300es:hlsl4:metal_macos -f sokol_impl
        COMMAND ${EFX_SHDC} -i ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post.glsl
                -o ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post.h
                --slang glsl410:glsl300es:hlsl4:metal_macos -f sokol_impl
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/quad.glsl
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.glsl
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post.glsl
        COMMENT "Regenerating shaders/quad.h, shaders/mesh.h and shaders/post.h via sokol-shdc")
endif()

enable_testing()
add_subdirectory(tests)

# Headless test executables share the src include, -lm on UNIX, Emscripten
# memory growth and the strict warning set. Every per-target difference stays
# an explicit argument: DEFS / MSVC_DEFS / POSIX_DEFS / INCLUDES /
# SYSTEM_INCLUDES / LIBS / MSVC_WARN (e.g. /W3 where a vendor header is not
# /W4-clean), DESKTOP_ONLY for the filesystem- or quickjs-bound suites, and
# NO_EM_GROWTH where a suite deliberately keeps the fixed-heap link line.
function(efx_add_test_exe name)
    cmake_parse_arguments(TEST "DESKTOP_ONLY;NO_EM_GROWTH" "MSVC_WARN"
        "SOURCES;LIBS;DEFS;MSVC_DEFS;POSIX_DEFS;INCLUDES;SYSTEM_INCLUDES" ${ARGN})
    if(TEST_DESKTOP_ONLY AND EMSCRIPTEN)
        return()
    endif()
    add_executable(${name} ${TEST_SOURCES})
    target_include_directories(${name} PRIVATE src)
    if(TEST_INCLUDES)
        target_include_directories(${name} PRIVATE ${TEST_INCLUDES})
    endif()
    if(TEST_SYSTEM_INCLUDES)
        target_include_directories(${name} SYSTEM PRIVATE ${TEST_SYSTEM_INCLUDES})
    endif()
    if(TEST_DEFS)
        target_compile_definitions(${name} PRIVATE ${TEST_DEFS})
    endif()
    if(MSVC AND TEST_MSVC_DEFS)
        target_compile_definitions(${name} PRIVATE ${TEST_MSVC_DEFS})
    endif()
    if(NOT MSVC AND TEST_POSIX_DEFS)
        target_compile_definitions(${name} PRIVATE ${TEST_POSIX_DEFS})
    endif()
    if(TEST_LIBS)
        target_link_libraries(${name} PRIVATE ${TEST_LIBS})
    endif()
    if(UNIX)
        target_link_libraries(${name} PRIVATE m)
    endif()
    if(EMSCRIPTEN AND NOT TEST_NO_EM_GROWTH)
        target_link_options(${name} PRIVATE -sALLOW_MEMORY_GROWTH=1)
    endif()
    if(MSVC)
        if(TEST_MSVC_WARN)
            target_compile_options(${name} PRIVATE ${TEST_MSVC_WARN})
        else()
            target_compile_options(${name} PRIVATE /W4 /WX)
        endif()
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

# headless math wrapper tests (GLM behind the plain C API, ADR 0005); GLM
# headers are not /W4-clean and the suite keeps the fixed-heap link line
efx_add_test_exe(efx_math_tests
    SOURCES src/math/efx_math.cpp tests/unit/math_tests.c
    SYSTEM_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    MSVC_WARN /W3
    NO_EM_GROWTH
)

# headless display-list unit tests (ADR 0019 record/assert gate)
efx_add_test_exe(efx_render_tests
    SOURCES ${EFX_RENDER_CORE_SOURCES} tests/unit/render_tests.c
)

# headless input-core unit tests (F9): pure-C state model + injection seam
efx_add_test_exe(efx_input_tests
    SOURCES ${EFX_INPUT_SOURCES} tests/unit/input_tests.c
)

# headless physics-core tests (F12): a self-contained C module — its own math,
# no renderer, platform layer, script runtime, or GLM — so this binary builds
# and runs in milliseconds on every target
efx_add_test_exe(efx_physics_tests
    SOURCES ${EFX_PHYSICS_SOURCES}
            tests/physics/main.c
            tests/physics/tests_narrow.c
            tests/physics/tests_world.c
            tests/physics/tests_character.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/tests/physics
)

# headless audio-core tests (F14): pure-C mixer/decoder/voice bank with no
# device. The WAV and MP3 fixtures are embedded at configure time so the suite
# is portable (no filesystem) and runs on Emscripten too.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/audio/tone.wav EFX_AUDIO_WAV_HEX HEX)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/audio/tone.mp3 EFX_AUDIO_MP3_HEX HEX)
string(LENGTH "${EFX_AUDIO_WAV_HEX}" EFX_AUDIO_WAV_HEXLEN)
math(EXPR EFX_AUDIO_WAV_SIZE "${EFX_AUDIO_WAV_HEXLEN} / 2")
string(LENGTH "${EFX_AUDIO_MP3_HEX}" EFX_AUDIO_MP3_HEXLEN)
math(EXPR EFX_AUDIO_MP3_SIZE "${EFX_AUDIO_MP3_HEXLEN} / 2")
string(REGEX REPLACE "(..)" "0x\\1," EFX_AUDIO_WAV_BYTES "${EFX_AUDIO_WAV_HEX}")
string(REGEX REPLACE "(..)" "0x\\1," EFX_AUDIO_MP3_BYTES "${EFX_AUDIO_MP3_HEX}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/efx_audio_fixtures.h
"#ifndef EFX_AUDIO_FIXTURES_H\n#define EFX_AUDIO_FIXTURES_H\nstatic const unsigned char EFX_TEST_WAV[] = {${EFX_AUDIO_WAV_BYTES}};\nstatic const unsigned int EFX_TEST_WAV_SIZE = ${EFX_AUDIO_WAV_SIZE};\nstatic const unsigned char EFX_TEST_MP3[] = {${EFX_AUDIO_MP3_BYTES}};\nstatic const unsigned int EFX_TEST_MP3_SIZE = ${EFX_AUDIO_MP3_SIZE};\n#endif\n")

efx_add_test_exe(efx_audio_tests
    SOURCES ${EFX_AUDIO_SOURCES} tests/unit/audio_tests.c
    INCLUDES ${CMAKE_CURRENT_BINARY_DIR}
    LIBS efx_drlibs
    MSVC_DEFS _CRT_SECURE_NO_WARNINGS
)

# headless resource provider tests (F6a directory backend + path safety);
# desktop-only: they read the filesystem
efx_add_test_exe(efx_resource_tests
    SOURCES ${EFX_RENDER_CORE_SOURCES} ${EFX_RESOURCE_SOURCES} tests/unit/resource_tests.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
    LIBS efx_miniz efx_cgltf
    DEFS EFX_RES_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/resource"
    DESKTOP_ONLY
)

# headless font/text tests (F8a): pure-C font module + display-list records;
# desktop-only: they read a TTF fixture from the filesystem
efx_add_test_exe(efx_text_tests
    SOURCES ${EFX_RENDER_CORE_SOURCES} ${EFX_TEXT_SOURCES} src/resource/resource.c
            tests/unit/text_tests.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
    LIBS efx_miniz
    DEFS EFX_FONT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/resource"
    MSVC_DEFS _CRT_SECURE_NO_WARNINGS
    POSIX_DEFS _POSIX_C_SOURCE=200809L
    DESKTOP_ONLY
)

# headless JS-API tests (mock GPU sink + real quickjs runtime + bindings);
# desktop-only: ADR 0022 keeps quickjs out of every Emscripten target
efx_add_test_exe(efx_api_tests
    SOURCES ${EFX_API_SOURCES}
            ${EFX_RENDER_CORE_SOURCES}
            ${EFX_TEXT_SOURCES}
            ${EFX_AUDIO_SOURCES}
            ${EFX_INPUT_SOURCES}
            ${EFX_PHYSICS_SOURCES}
            ${EFX_RESOURCE_SOURCES}
            tests/unit/api_tests.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
    LIBS qjs efx_miniz efx_cgltf efx_drlibs
    DEFS EFX_RES_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/resource"
         EFX_MOD_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/modules"
         EFX_MOD_ERROR_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/modules_error"
         EFX_AUDIO_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/audio"
    MSVC_DEFS _CRT_SECURE_NO_WARNINGS
    POSIX_DEFS _POSIX_C_SOURCE=200809L
    DESKTOP_ONLY
)

# headless desktop runner for the error catalog (ADR 0049): the player's
# --script output contract over a mock GPU sink, so the catalog's
# byte-compare runs in EFX_HEADLESS builds too; desktop-only (quickjs
# runtime, ADR 0022)
if(NOT EMSCRIPTEN)
    add_executable(efx_catalog_runner tests/catalog_runner.c)
    target_link_libraries(efx_catalog_runner PRIVATE efx_core)
    if(MSVC)
        target_compile_definitions(efx_catalog_runner PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(efx_catalog_runner PRIVATE -Wall -Wextra)
        target_compile_definitions(efx_catalog_runner PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
endif()

if(EFX_BUILD_DEV_HARNESS AND NOT EMSCRIPTEN)
    add_executable(efx_dev_harness
        tests/dev_harness.c
        src/player/player.c
        src/player/repl.c
    )
    target_link_libraries(efx_dev_harness PRIVATE efx_core efx_platform)
    if(MSVC)
        target_compile_definitions(efx_dev_harness PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(efx_dev_harness PRIVATE /W4 /WX)
    else()
        target_compile_options(efx_dev_harness PRIVATE -Wall -Wextra -Werror)
        if(UNIX)
            target_compile_definitions(efx_dev_harness PRIVATE _POSIX_C_SOURCE=200809L)
        endif()
    endif()
endif()
