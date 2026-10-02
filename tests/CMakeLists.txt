set(S ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(F ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

# EFX_HEADLESS (root CMakeLists): no player target — register only the
# headless unit-test cases at the bottom of this file
if(NOT EFX_HEADLESS)
# The one place that knows how a player run becomes a ctest invocation: all
# argument escaping and the run_test.cmake plumbing. add_player_test and
# add_web_test are thin wrappers supplying the runtime-specific -D set;
# MIDDLE_DARGS carries those -D entries that sit between PLAYER_UNDER_NODE
# and -DARG_LINE (web staging).
function(efx_add_run_test NAME PLAYER_NODE MIDDLE_DARGS)
    cmake_parse_arguments(ARG "" "EXPECT;EXPECT_OUT;EXPECT_OUT_FILE;EXPECT_ERR;INPUT_FILE" "ARGS" ${ARGN})
    set(ARG_LINE "${ARG_ARGS}")
    string(REPLACE ";" "\\;" ARG_LINE_ESC "${ARG_LINE}")
    string(REPLACE ";" "\\;" EXPECT_OUT_ESC "${ARG_EXPECT_OUT}")
    string(REPLACE ";" "\\;" EXPECT_ERR_ESC "${ARG_EXPECT_ERR}")
    set(INPUT_ARG "")
    if(ARG_INPUT_FILE)
        set(INPUT_ARG "-DINPUT_FILE=${ARG_INPUT_FILE}")
    endif()
    add_test(NAME ${NAME}
        COMMAND ${CMAKE_COMMAND}
            -DTEST_NAME=${NAME}
            -DPLAYER_COMMAND=$<TARGET_FILE:player>
            -DPLAYER_UNDER_NODE=${PLAYER_NODE}
            ${MIDDLE_DARGS}
            -DARG_LINE=${ARG_LINE_ESC}
            -DEXPECT=${ARG_EXPECT}
            -DEXPECT_OUT=${EXPECT_OUT_ESC}
            -DEXPECT_OUT_FILE=${ARG_EXPECT_OUT_FILE}
            -DEXPECT_ERR=${EXPECT_ERR_ESC}
            ${INPUT_ARG}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
endfunction()

function(add_player_test NAME)
    cmake_parse_arguments(ARG "" "EXPECT;EXPECT_OUT;EXPECT_OUT_FILE;EXPECT_ERR;INPUT_FILE" "ARGS" ${ARGN})
    efx_add_run_test(${NAME} $<BOOL:${EMSCRIPTEN}> ""
        ARGS ${ARG_ARGS}
        EXPECT ${ARG_EXPECT}
        EXPECT_OUT "${ARG_EXPECT_OUT}"
        EXPECT_OUT_FILE "${ARG_EXPECT_OUT_FILE}"
        EXPECT_ERR "${ARG_EXPECT_ERR}"
        INPUT_FILE "${ARG_INPUT_FILE}")
endfunction()

# web native-runtime tests (ADR 0022): the host JS engine (Node in this
# harness) runs the same portable scripts through the resource-root entry,
# asserting the same observable effects as the desktop --script suite
function(add_web_test NAME STAGE_SCRIPT)
    cmake_parse_arguments(ARG "" "EXPECT;EXPECT_OUT;EXPECT_OUT_FILE;EXPECT_ERR;ROOT" "ARGS;ASSETS" ${ARGN})
    string(REPLACE ";" "\\;" STAGE_ASSETS_ESC "${ARG_ASSETS}")
    set(MIDDLE "-DSTAGE_ASSETS=${STAGE_ASSETS_ESC}")
    if(ARG_ROOT)
        list(PREPEND MIDDLE "-DROOT_PATH=${ARG_ROOT}")
    elseif(IS_DIRECTORY "${STAGE_SCRIPT}")
        list(PREPEND MIDDLE "-DROOT_PATH=${STAGE_SCRIPT}")
    else()
        list(PREPEND MIDDLE "-DSTAGE_SCRIPT=${STAGE_SCRIPT}" "-DBINARY_DIR=${CMAKE_BINARY_DIR}")
    endif()
    efx_add_run_test(${NAME} ON "${MIDDLE}"
        ARGS ${ARG_ARGS}
        EXPECT ${ARG_EXPECT}
        EXPECT_OUT "${ARG_EXPECT_OUT}"
        EXPECT_OUT_FILE "${ARG_EXPECT_OUT_FILE}"
        EXPECT_ERR "${ARG_EXPECT_ERR}")
endfunction()

# One row per case that desktop and web run with identical arguments and
# expectations (refactor-volume-build D2): registers smoke_<name> on desktop
# and web_<name> on Emscripten. PATH is the --script file, or the resource
# root with ROOT; extra ARGS follow the path on both runtimes. Cases that
# genuinely differ between the runtimes stay explicit
# add_player_test/add_web_test rows.
function(efx_portable_case NAME PATH)
    cmake_parse_arguments(ARG "ROOT" "EXPECT;EXPECT_OUT;EXPECT_ERR" "ARGS" ${ARGN})
    if(NOT EMSCRIPTEN)
        if(ARG_ROOT)
            set(PLAYER_ARGS "${PATH}")
        else()
            set(PLAYER_ARGS "--script;${PATH}")
        endif()
        if(ARG_ARGS)
            list(APPEND PLAYER_ARGS ${ARG_ARGS})
        endif()
        add_player_test(smoke_${NAME}
            ARGS ${PLAYER_ARGS}
            EXPECT ${ARG_EXPECT}
            EXPECT_OUT "${ARG_EXPECT_OUT}"
            EXPECT_ERR "${ARG_EXPECT_ERR}")
    else()
        add_web_test(web_${NAME} "${PATH}"
            ARGS ${ARG_ARGS}
            EXPECT ${ARG_EXPECT}
            EXPECT_OUT "${ARG_EXPECT_OUT}"
            EXPECT_ERR "${ARG_EXPECT_ERR}")
    endif()
endfunction()

if(NOT EMSCRIPTEN)
add_player_test(smoke_log ARGS "--script;${S}/s_log.js" EXPECT 0)
add_player_test(smoke_quit3 ARGS "--script;${S}/s_quit3.js" EXPECT 3)
add_player_test(smoke_missing_file ARGS "--script;${S}/does_not_exist.js" EXPECT nonzero EXPECT_ERR "cannot read script file")
add_player_test(smoke_no_script_arg ARGS --script EXPECT nonzero EXPECT_ERR "requires a file argument")
add_player_test(smoke_root_ok ARGS "${F}/root_ok" EXPECT 0 EXPECT_OUT "entry-ok")
add_player_test(smoke_root_no_entry ARGS "${F}/root_no_entry" EXPECT nonzero EXPECT_ERR "no main.js in resource root")
add_player_test(smoke_root_missing_dir ARGS "${F}/no_such_dir" EXPECT nonzero EXPECT_ERR "not a directory")
add_player_test(smoke_root_eval_throw ARGS "${F}/web/root_eval_throw" EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
add_player_test(smoke_no_args EXPECT nonzero EXPECT_ERR "usage")
add_player_test(smoke_unknown_option ARGS --frobnicate EXPECT nonzero EXPECT_ERR "unknown option")
add_player_test(smoke_6a_resource ARGS "--script;${S}/s_6a_resource.js" EXPECT 0 EXPECT_OUT "s-6a-resource-ok")
add_player_test(smoke_6b_gltf ARGS "--script;${S}/s_6b_gltf.js" EXPECT 0 EXPECT_OUT "s-6b-gltf-ok")
add_player_test(smoke_7_skin_pose ARGS "--script;${S}/s_7_skin_pose.js;--root;${F}/resource/gltf" EXPECT 0 EXPECT_OUT "s-7-skin-pose-ok")
add_player_test(smoke_14_audio ARGS "--script;${S}/s_14_audio.js;--root;${F}/audio" EXPECT 0 EXPECT_OUT "s-14-audio-ok")
add_player_test(smoke_error_catalog ARGS "--script;${S}/s_error_catalog.js" EXPECT 0 EXPECT_OUT_FILE ${S}/s_error_catalog.expected.txt)
add_player_test(smoke_6a_root ARGS "--script;${S}/s_6a_root.js;--root;${F}/resource" EXPECT 0 EXPECT_OUT "s-6a-root-ok")
add_player_test(smoke_6a_root_zip ARGS "--script;${S}/s_6a_root_zip.js;--root;${F}/root_zip.zip" EXPECT 0 EXPECT_OUT "s-6a-root-zip-ok")
# gallery showcase samples run against their sample directories: a failed
# load throws and exits non-zero, so exit 0 proves the resources load on the
# player from the directory resource root (the directory is the pack)
set(GALLERY ${CMAKE_SOURCE_DIR}/gallery/samples/curated)
add_player_test(smoke_showcase_texture ARGS "--script;${GALLERY}/texture-showcase/main.js;--root;${GALLERY}/texture-showcase" EXPECT 0)
add_player_test(smoke_showcase_gltf ARGS "--script;${GALLERY}/gltf-showcase/main.js;--root;${GALLERY}/gltf-showcase" EXPECT 0)
add_player_test(smoke_showcase_fox ARGS "--script;${GALLERY}/fox-walk/main.js;--root;${GALLERY}/fox-walk" EXPECT 0)
add_player_test(smoke_showcase_modules ARGS "--script;${GALLERY}/modules-showcase/main.js;--root;${GALLERY}/modules-showcase" EXPECT 0)
add_player_test(smoke_showcase_text ARGS "--script;${GALLERY}/text-showcase/main.js;--root;${GALLERY}/text-showcase" EXPECT 0)
add_player_test(smoke_showcase_physics ARGS "--script;${GALLERY}/physics-showcase/main.js" EXPECT 0)
add_player_test(smoke_showcase_gamepad ARGS "--script;${GALLERY}/gamepad-tester/main.js;--root;${GALLERY}/gamepad-tester" EXPECT 0)
add_player_test(smoke_showcase_audio ARGS "--script;${GALLERY}/audio-showcase/main.js;--root;${GALLERY}/audio-showcase" EXPECT 0)
add_player_test(smoke_root_zip ARGS "${F}/root_zip.zip" EXPECT 0 EXPECT_OUT "zip-entry-ok")
add_player_test(smoke_repl_bad_root ARGS "--repl;${F}/no_such_dir" EXPECT nonzero EXPECT_ERR "not a directory")
endif()

if(EMSCRIPTEN)
add_web_test(web_log ${S}/s_log.js EXPECT 0 EXPECT_OUT "smoke-log-ok")
add_web_test(web_quit3 ${S}/s_quit3.js EXPECT 3 EXPECT_OUT "before-quit")
set(WEBF ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/web)
add_web_test(web_root_ok ${F}/root_ok EXPECT 0 EXPECT_OUT "entry-ok")
add_web_test(web_root_no_entry ${F}/root_no_entry EXPECT nonzero EXPECT_ERR "no main.js in resource root")
add_web_test(web_root_missing_dir missing EXPECT nonzero EXPECT_ERR "not a directory" ROOT ${F}/no_such_dir)
add_web_test(web_root_frame ${WEBF}/root_frame EXPECT 0 EXPECT_OUT "frame-1")
add_web_test(web_root_hooks_order ${WEBF}/root_hooks_order EXPECT 0 EXPECT_OUT "u1.*r1.*u2")
add_web_test(web_explicit_hooks ${WEBF}/root_explicit_hooks EXPECT 0 EXPECT_OUT "u1.*u2.*r1.*u2.*r2.*done")
add_web_test(web_root_hook_throw ${WEBF}/root_hook_throw EXPECT nonzero EXPECT_ERR "uncaught exception: hook boom")
add_web_test(web_root_eval_throw ${WEBF}/root_eval_throw EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
add_web_test(web_6b_gltf ${F}/web/gltf_probe EXPECT 0 EXPECT_OUT "s-6b-gltf-ok")
add_web_test(web_pose ${WEBF}/skin_probe EXPECT 0 EXPECT_OUT "s-7-skin-pose-ok")
add_web_test(web_14_audio ${F}/audio EXPECT 0 EXPECT_OUT "s-14-audio-ok")
add_web_test(web_error_catalog ${S}/s_error_catalog.js EXPECT 0 EXPECT_OUT_FILE ${S}/s_error_catalog.expected.txt ASSETS ${S}/skin.gltf ${S}/skin.bin ${S}/gltf_probe.glb ${S}/gltf_corrupt.gltf)
add_web_test(web_repl_unavailable "" EXPECT nonzero EXPECT_ERR "repl mode is unavailable" ARGS --repl)
endif()

# portable cases: identical script/root, args and expectations on both
# runtimes (one row instead of a mirrored smoke_*/web_* pair)
efx_portable_case(args "${S}/s_args.js" ARGS one two EXPECT 0)
efx_portable_case(portable "${S}/s_portable.js" EXPECT 0)
efx_portable_case(throw "${S}/s_throw.js" EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
efx_portable_case(2d_validation "${S}/s_2d_validation.js" EXPECT 0)
efx_portable_case(resource_lifecycle "${S}/s_resource_lifecycle.js" EXPECT 0)
efx_portable_case(3d_validation "${S}/s_3d_validation.js" EXPECT 0 EXPECT_OUT "s-3d-validation-ok")
efx_portable_case(3d_math "${S}/s_3d_math.js" EXPECT 0 EXPECT_OUT "s-3d-math-ok")
efx_portable_case(4a_validation "${S}/s_4a_validation.js" EXPECT 0 EXPECT_OUT "s-4a-validation-ok")
efx_portable_case(4b_validation "${S}/s_4b_validation.js" EXPECT 0 EXPECT_OUT "s-4b-validation-ok")
efx_portable_case(5a_validation "${S}/s_5a_validation.js" EXPECT 0 EXPECT_OUT "s-5a-validation-ok")
efx_portable_case(5b_validation "${S}/s_5b_validation.js" EXPECT 0 EXPECT_OUT "s-5b-validation-ok")
efx_portable_case(6c_skin "${S}/s_6c_skin.js" EXPECT 0 EXPECT_OUT "s-6c-skin-ok")
efx_portable_case(9_input "${S}/s_9_input.js" EXPECT 0 EXPECT_OUT "s-9-input-ok")
efx_portable_case(10_modules "${F}/modules" ROOT EXPECT 0 EXPECT_OUT "s-10-modules-ok")
efx_portable_case(10_module_throw "${F}/modules_error" ROOT EXPECT nonzero EXPECT_ERR "intentional-module-throw")
efx_portable_case(10_missing "${S}/s_10_missing.js" EXPECT nonzero EXPECT_ERR "cannot find module")
efx_portable_case(10_escape "${S}/s_10_escape.js" EXPECT nonzero EXPECT_ERR "escapes the resource root")
efx_portable_case(10_unsupported "${S}/s_10_unsupported.js" EXPECT nonzero)
efx_portable_case(10_nohost "${S}/s_10_nohost.js" EXPECT 0 EXPECT_OUT "s-10-nohost-ok")
efx_portable_case(11_particles "${S}/s_11_particles.js" EXPECT 0 EXPECT_OUT "s-11-particles-ok")
efx_portable_case(12_physics "${S}/physics_smoke.js" EXPECT 0 EXPECT_OUT "s-12-physics-ok")
efx_portable_case(13_gamepad "${S}/s_13_gamepad.js" EXPECT 0 EXPECT_OUT "s-13-gamepad-ok")
efx_portable_case(8a_text "${F}/web/text_root" ROOT EXPECT 0 EXPECT_OUT "s-8a-text-ok")
efx_portable_case(natives_privacy "${S}/s_natives_privacy.js" EXPECT 0
    EXPECT_OUT "s-natives-privacy-ok")

endif() # NOT EFX_HEADLESS

# headless unit tests (display list + JS API against a mock GPU sink;
# always registered — their targets exist in EFX_HEADLESS builds too)
function(add_unit_test TARGET CASE)
    if(EMSCRIPTEN)
        add_test(NAME ${TARGET}_${CASE}
            COMMAND node $<TARGET_FILE:${TARGET}> ${CASE})
    else()
        add_test(NAME ${TARGET}_${CASE} COMMAND ${TARGET} ${CASE})
    endif()
endfunction()

# one ctest per EFX_CASE(name) entry in the suite source — its only case list
function(efx_register_unit_cases TARGET SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE})
    file(STRINGS ${SOURCE} LINES REGEX "EFX_CASE\\(")
    foreach(LINE IN LISTS LINES)
        string(REGEX MATCHALL "EFX_CASE\\([A-Za-z0-9_]+\\)" HITS "${LINE}")
        foreach(HIT IN LISTS HITS)
            string(REGEX REPLACE "EFX_CASE\\(([A-Za-z0-9_]+)\\)" "\\1" CASE "${HIT}")
            add_unit_test(${TARGET} ${CASE})
        endforeach()
    endforeach()
endfunction()

set(U ${CMAKE_CURRENT_SOURCE_DIR}/unit)
efx_register_unit_cases(efx_math_tests ${U}/math_tests.c)
efx_register_unit_cases(efx_input_tests ${U}/input_tests.c)
efx_register_unit_cases(efx_physics_tests ${CMAKE_CURRENT_SOURCE_DIR}/physics/main.c)
efx_register_unit_cases(efx_audio_tests ${U}/audio_tests.c)
efx_register_unit_cases(efx_render_tests ${U}/render_tests.c)
# desktop-only: resource/text read the filesystem; api needs quickjs (ADR 0022)
if(NOT EMSCRIPTEN)
    efx_register_unit_cases(efx_resource_tests ${U}/resource_tests.c)
    efx_register_unit_cases(efx_text_tests ${U}/text_tests.c)
    efx_register_unit_cases(efx_api_tests ${U}/api_tests.c)
    if(EFX_HEADLESS)
        # no player target in headless builds: the error catalog's
        # byte-compare runs through efx_catalog_runner instead
        add_test(NAME catalog_headless
            COMMAND ${CMAKE_COMMAND}
                -DTEST_NAME=catalog_headless
                -DPLAYER_COMMAND=$<TARGET_FILE:efx_catalog_runner>
                -DARG_LINE=${S}/s_error_catalog.js\\;${S}
                -DEXPECT=0
                -DEXPECT_OUT_FILE=${S}/s_error_catalog.expected.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
        add_test(NAME natives_privacy_headless
            COMMAND ${CMAKE_COMMAND}
                -DTEST_NAME=natives_privacy_headless
                -DPLAYER_COMMAND=$<TARGET_FILE:efx_catalog_runner>
                -DARG_LINE=${S}/s_natives_privacy.js\\;${S}
                -DEXPECT=0
                -DEXPECT_OUT=s-natives-privacy-ok
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
    endif()
endif()

# golden-image tests (require a GPU/display; enable in CI or locally with a
# running window system) — Emscripten goldens run through the browser job
option(EFX_BUILD_GOLDEN_TESTS "Build/run golden-image capture tests" OFF)
if(EFX_BUILD_GOLDEN_TESTS AND NOT EMSCRIPTEN AND NOT EFX_HEADLESS)
    # embeds stb implementations -> relaxed warnings for this tool
    add_executable(efx_imgdiff imgdiff.c)
    target_include_directories(efx_imgdiff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb)
    if(UNIX)
        target_link_libraries(efx_imgdiff PRIVATE m)
    endif()
    if(MSVC)
        target_compile_options(efx_imgdiff PRIVATE /W3)
    else()
        target_compile_options(efx_imgdiff PRIVATE -Wall -Wno-unused-function)
    endif()

    # a scene's test activates once its golden image is committed (the
    # golden bootstrap produces them via the manual workflow job)
    foreach(SCENE clear solid srcrect pivot blend camera origin
                cube3d vertcolor3d surfaces3d depth3d transform3d plane3d
                light_ambient light_directional light_specular
                light_attenuation light_emissive light_multimaterial
                light_reference
                map_diffuse map_ambient map_specular map_emissive
                map_alpha_mask map_multimap map_reference
                rt_quad2d rt_scene3d rt_camera rt_materialmap
                post_colorfilter post_blur_small post_blur_large post_bloom
                post_mix post_scale_nearest post_scale_linear
                post_order_a post_order_b post_user_rt load_png gltf_import
                skin_import skin_pose mipmaps text_basic text_effects
                particles)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/goldens/${SCENE}/golden.png)
            add_test(NAME golden_${SCENE}
                COMMAND ${CMAKE_COMMAND}
                    -DGOLDEN_NAME=${SCENE}
                    -DGOLDEN_PLAYER=$<TARGET_FILE:player>
                    -DGOLDEN_IMGDIFF=$<TARGET_FILE:efx_imgdiff>
                    -DGOLDEN_SCENE=${CMAKE_CURRENT_SOURCE_DIR}/goldens/${SCENE}
                    -DBINARY_DIR=${CMAKE_BINARY_DIR}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/run_golden.cmake)
        else()
            message(STATUS "golden test deferred (no golden.png yet): ${SCENE}")
        endif()
    endforeach()

    # F6d: the interactive console needs a window like the golden suite; a
    # piped session drives evaluation, state persistence, root loading, error
    # recovery, `.help`/`.exit` and `efx.quit` exit codes (design D-risk)
    add_player_test(repl_session ARGS "--repl;${F}/repl_root"
        INPUT_FILE ${F}/repl/session.txt EXPECT 0
        EXPECT_OUT "repl-summary=true,repl-hello,5,true"
        EXPECT_ERR "intentional-repl-throw")
    add_player_test(repl_help ARGS "--repl;${F}/repl_root"
        INPUT_FILE ${F}/repl/session.txt EXPECT 0 EXPECT_OUT "REPL commands")
    add_player_test(repl_quit ARGS "--repl"
        INPUT_FILE ${F}/repl/quit.txt EXPECT 3 EXPECT_OUT "repl-before-quit")
endif()

if(EFX_BUILD_DEV_HARNESS AND NOT EMSCRIPTEN)
    function(add_harness_test NAME SCRIPT OUT_RE ERR_RE)
        string(REPLACE ";" "\\;" ARG_LINE_ESC "--frames;12;${SCRIPT}")
        string(REPLACE ";" "\\;" OUT_RE_ESC "${OUT_RE}")
        string(REPLACE ";" "\\;" ERR_RE_ESC "${ERR_RE}")
        add_test(NAME ${NAME}
            COMMAND ${CMAKE_COMMAND}
                -DTEST_NAME=${NAME}
                -DPLAYER_COMMAND=$<TARGET_FILE:efx_dev_harness>
                -DARG_LINE=${ARG_LINE_ESC}
                -DEXPECT=0
                -DEXPECT_OUT=${OUT_RE_ESC}
                -DEXPECT_ERR=${ERR_RE_ESC}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
    endfunction()

    add_harness_test(hooks_order_and_quit ${S}/h_both.js
        "u1.*r1.*u3-quit"
        "hooks update=1 render=1.*quit=1 code=5 error=0")
    add_harness_test(hooks_render_optional ${S}/h_only_update.js
        ""
        "hooks update=1 render=0.*quit=1 code=0 error=0")
    add_harness_test(hooks_throw_stops ${S}/h_throw.js
        ""
        "uncaught exception: hook boom.*error=1")
    add_harness_test(hooks_explicit_order ${S}/h_explicit_order.js
        "eu1.*er1.*eu3-quit"
        "hooks update=1 render=1.*quit=1 code=5 error=0")
    add_harness_test(hooks_explicit_unsub ${S}/h_explicit_unsub.js
        "a.*unsub"
        "hooks update=1 render=0.*quit=1 code=0 error=0")
endif()
