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

# Tests only when this is the project being built. Consumed through
# FetchContent -- which is how HydroCoupleComposer and openswmm.gui take it
# -- the parent's manifest governs what is available, and gtest need not be
# among it.
option(HYDROCOUPLEOGC_BUILD_TESTS "Build the test suite"
       ${PROJECT_IS_TOP_LEVEL})

find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_library(HydroCoupleOgc SHARED
    include/hydrocoupleogc/export.h
    include/hydrocoupleogc/axisorder.h
    include/hydrocoupleogc/crsurn.h
    include/hydrocoupleogc/httpclient.h
    include/hydrocoupleogc/servicecredentials.h
    include/hydrocoupleogc/servicediscovery.h
    include/hydrocoupleogc/wfscapabilities.h
    include/hydrocoupleogc/wfsrequest.h
    include/hydrocoupleogc/wmscapabilities.h
    include/hydrocoupleogc/wmsrequest.h
    include/hydrocoupleogc/wmtscapabilities.h
    src/axisorder.cpp
    src/crsurn.cpp
    src/httpclient.cpp
    src/servicecredentials.cpp
    src/servicediscovery.cpp
    src/wfscapabilities.cpp
    src/wfsrequest.cpp
    src/wmscapabilities.cpp
    src/wmsrequest.cpp
    src/wmtscapabilities.cpp
)

add_library(HydroCoupleOgc::HydroCoupleOgc ALIAS HydroCoupleOgc)

target_compile_definitions(HydroCoupleOgc
    PRIVATE
        HYDROCOUPLEOGC_LIBRARY
        # This library compiles with the Qt keywords off, because
        # HydroCoupleComposer must -- HydroCouple's ISignal declares a
        # method named emit -- and code that builds both ways is code
        # either project can take.
        #
        # PRIVATE, emphatically. Exported, it forces the setting on every
        # consumer: openswmm.gui uses `slots:` and `emit` throughout, and
        # inherited this and stopped compiling. A library may require
        # something of itself without requiring it of its callers, and its
        # own headers use none of the keywords, so nothing here needs it.
        QT_NO_KEYWORDS)

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

# The package config is for consumers that find_package() an installed
# copy. A FetchContent consumer links the build-tree target directly and
# needs none of it, but installing it costs nothing and keeps both routes
# open.
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
