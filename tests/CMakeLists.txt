# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(GTest CONFIG REQUIRED)

# The fixtures are real GetCapabilities responses saved from public servers,
# kept in the source tree so a failure can be opened and read.
function(hydrocoupleogc_add_test _name)
    add_executable(${_name} ${_name}.cpp)
    target_link_libraries(${_name}
        PRIVATE HydroCoupleOgc GTest::gtest GTest::gtest_main)
    target_compile_definitions(${_name} PRIVATE
        HYDROCOUPLEOGC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${_name} COMMAND ${_name})
    set_tests_properties(${_name} PROPERTIES TIMEOUT 60)
endfunction()

hydrocoupleogc_add_test(test_wmscapabilities)
hydrocoupleogc_add_test(test_wmtscapabilities)
