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

# F6a zip backend: miniz is a vendored single-file amalgamation built as its
# own target with relaxed warnings so its diagnostics never trip efx_core's
# -Werror (same treatment as the vendored stb implementation TUs).
add_library(efx_miniz STATIC vendor/miniz/miniz.c)
target_include_directories(efx_miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniz)
if(MSVC)
    target_compile_options(efx_miniz PRIVATE /W3)
else()
    target_compile_options(efx_miniz PRIVATE -w)
    # miniz uses ftello/fseeko (large-file stdio) on POSIX
    target_compile_definitions(efx_miniz PRIVATE _POSIX_C_SOURCE=200809L)
endif()

# F6b glTF parser: cgltf is one header with its implementation compiled in a
# dedicated TU with warnings relaxed (same treatment as the miniz/stb TUs).
add_library(efx_cgltf STATIC src/resource/cgltf_impl.c)
target_include_directories(efx_cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cgltf)
if(MSVC)
    target_compile_options(efx_cgltf PRIVATE /W3)
else()
    target_compile_options(efx_cgltf PRIVATE -w)
endif()

# F14 audio decoders: dr_libs (dr_wav + dr_mp3) is a single-header pair with
# its implementation compiled in a dedicated TU with warnings relaxed (same
# treatment as the miniz/cgltf/stb TUs).
add_library(efx_drlibs STATIC src/audio/dr_impl.c)
target_include_directories(efx_drlibs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dr_libs)
if(MSVC)
    target_compile_options(efx_drlibs PRIVATE /W3)
    target_compile_definitions(efx_drlibs PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(efx_drlibs PRIVATE -w)
endif()

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
    add_library(efx_core STATIC
        src/render/render_records.c
        src/render/render_texture.c
        src/render/render_target.c
        src/render/render_post.c
        src/render/render_mesh.c
        src/render/render_particles.c
        src/render/skin.c
        src/render/text.c
        src/input/efx_input.c
        src/input/efx_gamepad.c
        src/physics/shape.c
        src/physics/broadphase.c
        src/physics/narrow.c
        src/physics/solver.c
        src/physics/character.c
        src/physics/world.c
        src/audio/audio.c
        src/audio/decode.c
        src/resource/image.c
        src/resource/resource.c
        src/resource/gltf.c
        src/resource/stb_image_impl.c
        src/render/stb_truetype_impl.c
        src/render/stb_rect_pack_impl.c
    )
else()
    add_library(efx_core STATIC
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
        src/render/render_records.c
        src/render/render_texture.c
        src/render/render_target.c
        src/render/render_post.c
        src/render/render_mesh.c
        src/render/render_particles.c
        src/render/skin.c
        src/render/text.c
        src/input/efx_input.c
        src/input/efx_gamepad.c
        src/physics/shape.c
        src/physics/broadphase.c
        src/physics/narrow.c
        src/physics/solver.c
        src/physics/character.c
        src/physics/world.c
        src/audio/audio.c
        src/audio/decode.c
        src/resource/image.c
        src/resource/resource.c
        src/resource/gltf.c
        src/resource/stb_image_impl.c
        src/render/stb_truetype_impl.c
        src/render/stb_rect_pack_impl.c
    )
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

    set(EFX_WEB_COMMON
        -sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=2MB
        -sEXPORTED_FUNCTIONS=_main,_malloc,_free
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

# headless math wrapper tests (GLM behind the plain C API, ADR 0005)
add_executable(efx_math_tests
    src/math/efx_math.cpp
    tests/unit/math_tests.c
)
target_include_directories(efx_math_tests PRIVATE src)
target_include_directories(efx_math_tests SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
if(UNIX)
    target_link_libraries(efx_math_tests PRIVATE m)
endif()
if(MSVC)
    target_compile_options(efx_math_tests PRIVATE /W3)
else()
    target_compile_options(efx_math_tests PRIVATE -Wall -Wextra -Werror)
endif()

# headless display-list unit tests (ADR 0019 record/assert gate)
add_executable(efx_render_tests
    src/render/render_records.c
    src/render/render_texture.c
    src/render/render_target.c
    src/render/render_post.c
    src/render/render_mesh.c
    src/render/render_particles.c
    src/render/skin.c
    tests/unit/render_tests.c
)
target_include_directories(efx_render_tests PRIVATE src)
if(UNIX)
    target_link_libraries(efx_render_tests PRIVATE m)
endif()
if(EMSCRIPTEN)
    target_link_options(efx_render_tests PRIVATE -sALLOW_MEMORY_GROWTH=1)
endif()
if(MSVC)
    target_compile_options(efx_render_tests PRIVATE /W4 /WX)
else()
    target_compile_options(efx_render_tests PRIVATE -Wall -Wextra -Werror)
endif()

# headless input-core unit tests (F9): pure-C state model + injection seam
add_executable(efx_input_tests
    src/input/efx_input.c
    src/input/efx_gamepad.c
    tests/unit/input_tests.c
)
target_include_directories(efx_input_tests PRIVATE src)
if(EMSCRIPTEN)
    target_link_options(efx_input_tests PRIVATE -sALLOW_MEMORY_GROWTH=1)
endif()
if(MSVC)
    target_compile_options(efx_input_tests PRIVATE /W4 /WX)
else()
    target_compile_options(efx_input_tests PRIVATE -Wall -Wextra -Werror)
endif()

# headless physics-core tests (F12): a self-contained C module — its own math,
# no renderer, platform layer, script runtime, or GLM — so this binary builds
# and runs in milliseconds on every target
add_executable(efx_physics_tests
    src/physics/shape.c
    src/physics/broadphase.c
    src/physics/narrow.c
    src/physics/solver.c
    src/physics/character.c
    src/physics/world.c
    tests/physics/main.c
    tests/physics/tests_narrow.c
    tests/physics/tests_world.c
    tests/physics/tests_character.c
)
target_include_directories(efx_physics_tests PRIVATE src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/physics)
if(UNIX)
    target_link_libraries(efx_physics_tests PRIVATE m)
endif()
if(EMSCRIPTEN)
    target_link_options(efx_physics_tests PRIVATE -sALLOW_MEMORY_GROWTH=1)
endif()
if(MSVC)
    target_compile_options(efx_physics_tests PRIVATE /W4 /WX)
else()
    target_compile_options(efx_physics_tests PRIVATE -Wall -Wextra -Werror)
endif()

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

add_executable(efx_audio_tests
    src/audio/audio.c
    src/audio/decode.c
    tests/unit/audio_tests.c
)
target_include_directories(efx_audio_tests PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(efx_audio_tests PRIVATE efx_drlibs)
if(UNIX)
    target_link_libraries(efx_audio_tests PRIVATE m)
endif()
if(EMSCRIPTEN)
    target_link_options(efx_audio_tests PRIVATE -sALLOW_MEMORY_GROWTH=1)
endif()
if(MSVC)
    target_compile_options(efx_audio_tests PRIVATE /W4 /WX)
    target_compile_definitions(efx_audio_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(efx_audio_tests PRIVATE -Wall -Wextra -Werror)
endif()

# headless resource provider tests (F6a directory backend + path safety);
# desktop-only: they read the filesystem
if(NOT EMSCRIPTEN)
add_executable(efx_resource_tests
    src/render/render_records.c
    src/render/render_texture.c
    src/render/render_target.c
    src/render/render_post.c
    src/render/render_mesh.c
    src/render/render_particles.c
    src/render/skin.c
    src/resource/gltf.c
    src/resource/image.c
    src/resource/resource.c
    src/resource/stb_image_impl.c
    tests/unit/resource_tests.c
)
target_include_directories(efx_resource_tests PRIVATE src)
target_include_directories(efx_resource_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb)
target_link_libraries(efx_resource_tests PRIVATE efx_miniz efx_cgltf)
if(UNIX)
    target_link_libraries(efx_resource_tests PRIVATE m)
endif()
target_compile_definitions(efx_resource_tests PRIVATE
    EFX_RES_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/resource")
if(MSVC)
    target_compile_options(efx_resource_tests PRIVATE /W4 /WX)
else()
    target_compile_options(efx_resource_tests PRIVATE -Wall -Wextra -Werror)
endif()

# headless font/text tests (F8a): pure-C font module + display-list records;
# desktop-only: they read a TTF fixture from the filesystem
if(NOT EMSCRIPTEN)
add_executable(efx_text_tests
    src/render/render_records.c
    src/render/render_texture.c
    src/render/render_target.c
    src/render/render_post.c
    src/render/render_mesh.c
    src/render/render_particles.c
    src/render/skin.c
    src/render/text.c
    src/render/stb_truetype_impl.c
    src/render/stb_rect_pack_impl.c
    src/resource/resource.c
    tests/unit/text_tests.c
)
target_include_directories(efx_text_tests PRIVATE src)
target_include_directories(efx_text_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb)
target_link_libraries(efx_text_tests PRIVATE efx_miniz)
if(UNIX)
    target_link_libraries(efx_text_tests PRIVATE m)
endif()
target_compile_definitions(efx_text_tests PRIVATE
    EFX_FONT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/resource")
if(MSVC)
    target_compile_options(efx_text_tests PRIVATE /W4 /WX)
    target_compile_definitions(efx_text_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(efx_text_tests PRIVATE -Wall -Wextra -Werror)
endif()
if(NOT MSVC)
    target_compile_definitions(efx_text_tests PRIVATE _POSIX_C_SOURCE=200809L)
endif()
endif()

# headless JS-API tests (mock GPU sink + real quickjs runtime + bindings);
# desktop-only: ADR 0022 keeps quickjs out of every Emscripten target
add_executable(efx_api_tests
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
    src/render/render_records.c
    src/render/render_texture.c
    src/render/render_target.c
    src/render/render_post.c
    src/render/render_mesh.c
    src/render/render_particles.c
    src/render/skin.c
    src/render/text.c
    src/render/stb_truetype_impl.c
    src/render/stb_rect_pack_impl.c
    src/audio/audio.c
    src/audio/decode.c
    src/input/efx_input.c
    src/input/efx_gamepad.c
        src/physics/shape.c
        src/physics/broadphase.c
        src/physics/narrow.c
        src/physics/solver.c
        src/physics/character.c
        src/physics/world.c
    src/resource/gltf.c
    src/resource/image.c
    src/resource/resource.c
    src/resource/stb_image_impl.c
    tests/unit/api_tests.c
)
target_include_directories(efx_api_tests PRIVATE src)
target_include_directories(efx_api_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb)
target_compile_definitions(efx_api_tests PRIVATE
    EFX_RES_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/resource"
    EFX_MOD_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/modules"
    EFX_MOD_ERROR_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/modules_error"
    EFX_AUDIO_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/audio")
if(UNIX)
    target_link_libraries(efx_api_tests PRIVATE m)
endif()
target_link_libraries(efx_api_tests PRIVATE qjs)
target_link_libraries(efx_api_tests PRIVATE efx_miniz)
target_link_libraries(efx_api_tests PRIVATE efx_cgltf)
target_link_libraries(efx_api_tests PRIVATE efx_drlibs)
if(MSVC)
    target_compile_options(efx_api_tests PRIVATE /W4 /WX)
    target_compile_definitions(efx_api_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(efx_api_tests PRIVATE -Wall -Wextra -Werror)
endif()
if(NOT MSVC)
    target_compile_definitions(efx_api_tests PRIVATE _POSIX_C_SOURCE=200809L)
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
