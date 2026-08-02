get_filename_component(_TEST_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
add_executable(${_TEST_NAME}_tests
    ${_TEST_NAME}_tests_main.cpp
    multivendor_lan_tests.cpp
    # libslic3r_gui references these symbols but they live in the application
    # target rather than in a library, so the test binary has to compile them
    # too (same set the Snapmaker_Orca executable pulls in).
    ${CMAKE_SOURCE_DIR}/src/common_func/common_func.cpp
    ${CMAKE_SOURCE_DIR}/src/bury_cfg/bury_point.cpp
    ${CMAKE_SOURCE_DIR}/src/sentry_wrapper/SentryWrapper.cpp
    ${CMAKE_SOURCE_DIR}/src/dev-utils/BaseException.cpp
    ${CMAKE_SOURCE_DIR}/src/dev-utils/StackWalker.cpp
    )

target_link_libraries(${_TEST_NAME}_tests test_common libslic3r_gui libslic3r)
# These tests include libslic3r_gui headers, which pull in wxWidgets.
target_include_directories(${_TEST_NAME}_tests PRIVATE ${ORCA_WXWIDGETS_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(${_TEST_NAME}_tests PRIVATE ${ORCA_WXWIDGETS_DEFINITIONS} BURY_EXPORTS)
if (SLIC3R_SENTRY)
    target_compile_definitions(${_TEST_NAME}_tests PRIVATE SLIC3R_SENTRY)
    target_link_libraries(${_TEST_NAME}_tests sentry::sentry)
endif ()
if (MSVC)
    target_link_libraries(${_TEST_NAME}_tests Setupapi.lib)
endif ()
set_property(TARGET ${_TEST_NAME}_tests PROPERTY FOLDER "tests")

if (WIN32)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    Snapmaker_Orca_copy_dlls(COPY_DLLS "Debug" "d" output_dlls_Debug)
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    Snapmaker_Orca_copy_dlls(COPY_DLLS "RelWithDebInfo" "" output_dlls_Release)
    else()
    Snapmaker_Orca_copy_dlls(COPY_DLLS "Release" "" output_dlls_Release)
    endif()
endif()

catch_discover_tests(${_TEST_NAME}_tests TEST_PREFIX "${_TEST_NAME}: ")
# add_test(${_TEST_NAME}_tests ${_TEST_NAME}_tests "${CATCH_EXTRA_ARGS} exclude:[NotWorking]")
