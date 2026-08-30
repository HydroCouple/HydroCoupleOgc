# HydroCoupleOgc

OGC web service clients — WMS, WMTS, WCS, WFS, XYZ and ArcGIS REST — shared by
[HydroCoupleComposer](../HydroCoupleComposer) and
[openswmm.gui](../openswmm.gui).

Qt only, deliberately. In both consuming projects GDAL is built without curl:
`/vsicurl` and `CPLHTTPFetch` are stubs and the WMS/WCS/WFS drivers are not
registered, so GDAL cannot fetch anything. It is still used, in the consumers,
to *decode* what this library fetches.

The library is in three tiers, and the boundary is what makes it testable:

| Tier | What | Depends on |
| --- | --- | --- |
| capabilities | `bytes -> struct` parsers, URL builders, CRS URN resolution | Qt6::Core |
| fetch | HTTP, caching, cancellation, credentials | Qt6::Network |
| layers | *not here* — each host has its own layer contract | — |

The first tier is where the protocol risk lives, so it is pure functions over
saved responses from real servers, and every test runs without a network.

## Building it

Dependencies come the same way they do in the consuming projects: Qt from a Qt
installation, everything else from vcpkg's manifest.

```sh
export VCPKG_ROOT=/path/to/vcpkg
export QT_ROOT_DIR=/path/to/Qt/6.9.3/macos
cmake --preset Darwin && cmake --build build/darwin && ctest --test-dir build/darwin
```

`vcpkg.json` declares Google Test behind a `tests` feature, which the presets
turn on. Qt is deliberately not a vcpkg dependency — the consumers both take it
from a Qt installation through `CMAKE_PREFIX_PATH`, and a second Qt built by
vcpkg would be a second Qt in the same process.

## Using it

Through FetchContent, which is how HydroCoupleComposer takes it:

```cmake
include(FetchContent)

set(HYDROCOUPLEOGC_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(HydroCoupleOgc
    GIT_REPOSITORY https://github.com/HydroCouple/HydroCoupleOgc.git
    GIT_TAG main
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(HydroCoupleOgc)

target_link_libraries(your_target PRIVATE HydroCoupleOgc::HydroCoupleOgc)
```

Set `FETCHCONTENT_SOURCE_DIR_HYDROCOUPLEOGC` to a local checkout to build
against that instead of a download — which is what makes editing this library
and its consumer in the same session possible. An installed copy also works:
`find_package(HydroCoupleOgc CONFIG REQUIRED)` finds the same target under the
same name.

The test suites are the repository's own business and are off unless this is
the project being built, so a consumer needs no Google Test of its own.

## Licence

LGPL-3.0-or-later.
