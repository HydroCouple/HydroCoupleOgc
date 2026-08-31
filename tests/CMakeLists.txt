# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(GTest CONFIG REQUIRED)

# The fixtures are real GetCapabilities responses saved from public servers,
# kept in the source tree so a failure can be opened and read.
function(hydrocoupleogc_add_test _name)
    # OWN_MAIN: the test writes its own main() because it needs a
    # QCoreApplication for the event loop the network runs on.
    cmake_parse_arguments(_arg "OWN_MAIN" "" "" ${ARGN})

    add_executable(${_name} ${_name}.cpp)
    target_link_libraries(${_name} PRIVATE HydroCoupleOgc GTest::gtest)

    if(NOT _arg_OWN_MAIN)
        target_link_libraries(${_name} PRIVATE GTest::gtest_main)
    endif()

    target_compile_definitions(${_name} PRIVATE
        HYDROCOUPLEOGC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        HYDROCOUPLEOGC_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-output")
    add_test(NAME ${_name} COMMAND ${_name})
    set_tests_properties(${_name} PROPERTIES TIMEOUT 60)
endfunction()

hydrocoupleogc_add_test(test_wmscapabilities)
hydrocoupleogc_add_test(test_servicediscovery)
hydrocoupleogc_add_test(test_wcs)
hydrocoupleogc_add_test(test_wfs)
hydrocoupleogc_add_test(test_wmsrequest)
hydrocoupleogc_add_test(test_wmtscapabilities)
hydrocoupleogc_add_test(test_httpclient OWN_MAIN)
