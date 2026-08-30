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

## Licence

LGPL-3.0-or-later.
