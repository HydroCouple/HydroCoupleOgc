# SPDX-License-Identifier: LGPL-3.0-or-later
# HydroCoupleOgc — OGC web service clients shared by HydroCoupleComposer and
# openswmm.gui.
#
# Qt only. GDAL is deliberately absent: in both consuming projects GDAL is
# built without curl, so its WMS/WCS/WFS drivers are not registered and
# CPLHTTPFetch is a stub. Everything here talks HTTP through Qt instead.

cmake_minimum_required(VERSION 3.21)

project(HydroCoupleOgc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(HYDROCOUPLEOGC_BUILD_TESTS "Build the test suite" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_library(HydroCoupleOgc SHARED
    include/hydrocoupleogc/export.h
    include/hydrocoupleogc/crsurn.h
    include/hydrocoupleogc/httpclient.h
    include/hydrocoupleogc/servicecredentials.h
    include/hydrocoupleogc/wmscapabilities.h
    include/hydrocoupleogc/wmsrequest.h
    include/hydrocoupleogc/wmtscapabilities.h
    src/crsurn.cpp
    src/httpclient.cpp
    src/servicecredentials.cpp
    src/wmscapabilities.cpp
    src/wmsrequest.cpp
    src/wmtscapabilities.cpp
)

add_library(HydroCoupleOgc::HydroCoupleOgc ALIAS HydroCoupleOgc)

target_compile_definitions(HydroCoupleOgc
    PRIVATE HYDROCOUPLEOGC_LIBRARY
    # The consuming projects disable the Qt keywords because HydroCouple's
    # ISignal declares a method named emit; this library must compile under
    # that setting, so it never uses them either.
    PUBLIC QT_NO_KEYWORDS)

target_include_directories(HydroCoupleOgc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(HydroCoupleOgc PUBLIC Qt6::Core Qt6::Network)

set_target_properties(HydroCoupleOgc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

if(HYDROCOUPLEOGC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS HydroCoupleOgc EXPORT HydroCoupleOgcTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")

install(EXPORT HydroCoupleOgcTargets
        FILE HydroCoupleOgcTargets.cmake
        NAMESPACE HydroCoupleOgc::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HydroCoupleOgc)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/HydroCoupleOgcConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HydroCoupleOgcConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HydroCoupleOgc)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HydroCoupleOgcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HydroCoupleOgcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HydroCoupleOgcConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HydroCoupleOgc)

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/HydroCoupleOgc)
