set(S ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(F ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

# EFX_HEADLESS (root CMakeLists): no player target — register only the
# headless unit-test cases at the bottom of this file
if(NOT EFX_HEADLESS)
function(add_player_test NAME)
    cmake_parse_arguments(ARG "" "EXPECT;EXPECT_OUT;EXPECT_ERR;INPUT_FILE" "ARGS" ${ARGN})
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
            -DPLAYER_UNDER_NODE=$<BOOL:${EMSCRIPTEN}>
            -DARG_LINE=${ARG_LINE_ESC}
            -DEXPECT=${ARG_EXPECT}
            -DEXPECT_OUT=${EXPECT_OUT_ESC}
            -DEXPECT_ERR=${EXPECT_ERR_ESC}
            ${INPUT_ARG}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
endfunction()

if(NOT EMSCRIPTEN)
add_player_test(smoke_log ARGS "--script;${S}/s_log.js" EXPECT 0)
add_player_test(smoke_quit3 ARGS "--script;${S}/s_quit3.js" EXPECT 3)
add_player_test(smoke_args ARGS "--script;${S}/s_args.js;one;two" EXPECT 0)
add_player_test(smoke_portable ARGS "--script;${S}/s_portable.js" EXPECT 0)
add_player_test(smoke_throw ARGS "--script;${S}/s_throw.js" EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
add_player_test(smoke_missing_file ARGS "--script;${S}/does_not_exist.js" EXPECT nonzero EXPECT_ERR "cannot read script file")
add_player_test(smoke_no_script_arg ARGS --script EXPECT nonzero EXPECT_ERR "requires a file argument")
add_player_test(smoke_root_ok ARGS "${F}/root_ok" EXPECT 0 EXPECT_OUT "entry-ok")
add_player_test(smoke_root_no_entry ARGS "${F}/root_no_entry" EXPECT nonzero EXPECT_ERR "no main.js in resource root")
add_player_test(smoke_root_missing_dir ARGS "${F}/no_such_dir" EXPECT nonzero EXPECT_ERR "not a directory")
add_player_test(smoke_root_eval_throw ARGS "${F}/web/root_eval_throw" EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
add_player_test(smoke_no_args EXPECT nonzero EXPECT_ERR "usage")
add_player_test(smoke_unknown_option ARGS --frobnicate EXPECT nonzero EXPECT_ERR "unknown option")
add_player_test(smoke_2d_validation ARGS "--script;${S}/s_2d_validation.js" EXPECT 0)
add_player_test(smoke_resource_lifecycle ARGS "--script;${S}/s_resource_lifecycle.js" EXPECT 0)
add_player_test(smoke_3d_validation ARGS "--script;${S}/s_3d_validation.js" EXPECT 0 EXPECT_OUT "s-3d-validation-ok")
add_player_test(smoke_3d_math ARGS "--script;${S}/s_3d_math.js" EXPECT 0 EXPECT_OUT "s-3d-math-ok")
add_player_test(smoke_4a_validation ARGS "--script;${S}/s_4a_validation.js" EXPECT 0 EXPECT_OUT "s-4a-validation-ok")
add_player_test(smoke_4b_validation ARGS "--script;${S}/s_4b_validation.js" EXPECT 0 EXPECT_OUT "s-4b-validation-ok")
add_player_test(smoke_5a_validation ARGS "--script;${S}/s_5a_validation.js" EXPECT 0 EXPECT_OUT "s-5a-validation-ok")
add_player_test(smoke_5b_validation ARGS "--script;${S}/s_5b_validation.js" EXPECT 0 EXPECT_OUT "s-5b-validation-ok")
add_player_test(smoke_6a_resource ARGS "--script;${S}/s_6a_resource.js" EXPECT 0 EXPECT_OUT "s-6a-resource-ok")
add_player_test(smoke_6b_gltf ARGS "--script;${S}/s_6b_gltf.js" EXPECT 0 EXPECT_OUT "s-6b-gltf-ok")
add_player_test(smoke_6c_skin ARGS "--script;${S}/s_6c_skin.js" EXPECT 0 EXPECT_OUT "s-6c-skin-ok")
add_player_test(smoke_6a_root ARGS "--script;${S}/s_6a_root.js;--root;${F}/resource" EXPECT 0 EXPECT_OUT "s-6a-root-ok")
add_player_test(smoke_6a_root_zip ARGS "--script;${S}/s_6a_root_zip.js;--root;${F}/root_zip.zip" EXPECT 0 EXPECT_OUT "s-6a-root-zip-ok")
add_player_test(smoke_root_zip ARGS "${F}/root_zip.zip" EXPECT 0 EXPECT_OUT "zip-entry-ok")
add_player_test(smoke_repl_bad_root ARGS "--repl;${F}/no_such_dir" EXPECT nonzero EXPECT_ERR "not a directory")
endif()

# web native-runtime tests (ADR 0022): the host JS engine (Node in this
# harness) runs the same portable scripts through the resource-root entry,
# asserting the same observable effects as the desktop --script suite
if(EMSCRIPTEN)
    function(add_web_test NAME STAGE_SCRIPT)
        cmake_parse_arguments(ARG "" "EXPECT;EXPECT_OUT;EXPECT_ERR;ROOT" "ARGS" ${ARGN})
        set(ARG_LINE "${ARG_ARGS}")
        string(REPLACE ";" "\\;" ARG_LINE_ESC "${ARG_LINE}")
        string(REPLACE ";" "\\;" EXPECT_OUT_ESC "${ARG_EXPECT_OUT}")
        string(REPLACE ";" "\\;" EXPECT_ERR_ESC "${ARG_EXPECT_ERR}")
        set(EXTRA_ARGS "")
        if(ARG_ROOT)
            list(APPEND EXTRA_ARGS "-DROOT_PATH=${ARG_ROOT}")
        elseif(IS_DIRECTORY "${STAGE_SCRIPT}")
            list(APPEND EXTRA_ARGS "-DROOT_PATH=${STAGE_SCRIPT}")
        else()
            list(APPEND EXTRA_ARGS "-DSTAGE_SCRIPT=${STAGE_SCRIPT}" "-DBINARY_DIR=${CMAKE_BINARY_DIR}")
        endif()
        add_test(NAME ${NAME}
            COMMAND ${CMAKE_COMMAND}
                -DTEST_NAME=${NAME}
                -DPLAYER_COMMAND=$<TARGET_FILE:player>
                -DPLAYER_UNDER_NODE=ON
                ${EXTRA_ARGS}
                -DARG_LINE=${ARG_LINE_ESC}
                -DEXPECT=${ARG_EXPECT}
                -DEXPECT_OUT=${EXPECT_OUT_ESC}
                -DEXPECT_ERR=${EXPECT_ERR_ESC}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
    endfunction()

    set(WEBF ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/web)
    add_web_test(web_root_ok ${F}/root_ok EXPECT 0 EXPECT_OUT "entry-ok")
    add_web_test(web_root_no_entry ${F}/root_no_entry EXPECT nonzero EXPECT_ERR "no main.js in resource root")
    add_web_test(web_root_missing_dir missing EXPECT nonzero EXPECT_ERR "not a directory" ROOT ${F}/no_such_dir)
    add_web_test(web_root_frame ${WEBF}/root_frame EXPECT 0 EXPECT_OUT "frame-1")
    add_web_test(web_root_hooks_order ${WEBF}/root_hooks_order EXPECT 0 EXPECT_OUT "u1.*r1.*u2")
    add_web_test(web_explicit_hooks ${WEBF}/root_explicit_hooks EXPECT 0 EXPECT_OUT "u1.*u2.*r1.*u2.*r2.*done")
    add_web_test(web_root_hook_throw ${WEBF}/root_hook_throw EXPECT nonzero EXPECT_ERR "uncaught exception: hook boom")
    add_web_test(web_root_eval_throw ${WEBF}/root_eval_throw EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
    add_web_test(web_log ${S}/s_log.js EXPECT 0 EXPECT_OUT "smoke-log-ok")
    add_web_test(web_quit3 ${S}/s_quit3.js EXPECT 3 EXPECT_OUT "before-quit")
    add_web_test(web_args ${S}/s_args.js ARGS one;two EXPECT 0)
    add_web_test(web_portable ${S}/s_portable.js EXPECT 0)
    add_web_test(web_throw ${S}/s_throw.js EXPECT nonzero EXPECT_ERR "intentional-smoke-throw")
    add_web_test(web_2d_validation ${S}/s_2d_validation.js EXPECT 0)
    add_web_test(web_resource_lifecycle ${S}/s_resource_lifecycle.js EXPECT 0)
    add_web_test(web_3d_validation ${S}/s_3d_validation.js EXPECT 0 EXPECT_OUT "s-3d-validation-ok")
    add_web_test(web_3d_math ${S}/s_3d_math.js EXPECT 0 EXPECT_OUT "s-3d-math-ok")
    add_web_test(web_4a_validation ${S}/s_4a_validation.js EXPECT 0 EXPECT_OUT "s-4a-validation-ok")
    add_web_test(web_4b_validation ${S}/s_4b_validation.js EXPECT 0 EXPECT_OUT "s-4b-validation-ok")
    add_web_test(web_5a_validation ${S}/s_5a_validation.js EXPECT 0 EXPECT_OUT "s-5a-validation-ok")
    add_web_test(web_5b_validation ${S}/s_5b_validation.js EXPECT 0 EXPECT_OUT "s-5b-validation-ok")
    add_web_test(web_6b_gltf ${F}/web/gltf_probe EXPECT 0 EXPECT_OUT "s-6b-gltf-ok")
    add_web_test(web_6c_skin ${S}/s_6c_skin.js EXPECT 0 EXPECT_OUT "s-6c-skin-ok")
    add_web_test(web_repl_unavailable "" EXPECT nonzero EXPECT_ERR "repl mode is unavailable" ARGS --repl)
endif()

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

foreach(CASE
    identity perspective_values ortho_values look_at_values mul_order
    translate_rotate_scale vec3_ops mvp_compose)
    add_unit_test(efx_math_tests ${CASE})
endforeach()

foreach(CASE
    compose_camera compose_quad value_snapshot default_camera_viewport
    blend_snapshot record_budget texture_lifecycle record_fields batching
    meshdata_validation meshdata_skinning mesh_lifecycle mesh_pending_upload
    mesh_record_fields mesh_record_order mesh_record_budget
    lights_state light_snapshot material_binding lighting_reference
    material_maps map_retention lighting_maps
    render_target_lifecycle target_deferred_release segmentation
    target_redirection feedback_guard
    post_registry post_fast_path post_render_scale post_surface_size
    post_user_target_raw)
    add_unit_test(efx_render_tests ${CASE})
endforeach()
if(NOT EMSCRIPTEN)
foreach(CASE dir_read dir_nested dir_missing dir_traversal dir_bad_root
              image_png image_jpeg image_corrupt
              zip_read zip_nested zip_image zip_missing zip_bad_root
              gltf_triangle gltf_zip gltf_transform gltf_materials
              gltf_accessors gltf_glb gltf_dedup gltf_errors
              gltf_skin gltf_skin_u8 gltf_skin_errors)
    add_unit_test(efx_resource_tests ${CASE})
endforeach()
endif()
if(NOT EMSCRIPTEN)
foreach(CASE
    white quad_record size_derivation origin_pivot quad_validation
    texture_size_getters camera_snapshot src_oob budget texture_lifecycle
    blend_snapshot default_camera hooks_registration meshdata_js
    meshdata_cap_js mesh_js camera3d_js f4a_js f4b_js f5a_js f5b_js
    resource_js createTexture_js gltf_js skin_js repl_eval)
    add_unit_test(efx_api_tests ${CASE})
endforeach()
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
                skin_import)
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
