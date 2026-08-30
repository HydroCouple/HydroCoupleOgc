// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   test_wmtscapabilities.cpp
 * \brief  Reading a WMTS pyramid, and asking it for a tile.
 *
 * The fixture is a real national basemap service, saved rather than written,
 * and it was chosen because it carries the two things a hand-written one
 * would not have thought to include: a coordinate system named with a
 * three-part register version, and RESTful URL templates published alongside
 * a query endpoint. Both are exactly where the implementation this replaces
 * goes wrong.
 */

#include "hydrocoupleogc/crsurn.h"
#include "hydrocoupleogc/wmtscapabilities.h"

#include <gtest/gtest.h>

#include <QFile>

#include <algorithm>

using namespace HydroCouple::Ogc;

namespace
{
  QByteArray fixture(const QString &name)
  {
    QFile file(QStringLiteral(HYDROCOUPLEOGC_FIXTURE_DIR "/") + name);

    if (!file.open(QIODevice::ReadOnly))
    {
      return {};
    }

    return file.readAll();
  }

  WmtsCapabilities vienna()
  {
    return parseWmtsCapabilities(
      fixture(QStringLiteral("wmts-1.0.0-resourceurl.xml")));
  }

  WmtsLayerInfo layerNamed(const WmtsCapabilities &capabilities,
                           const QString &identifier)
  {
    for (const WmtsLayerInfo &layer : capabilities.layers)
    {
      if (layer.identifier == identifier)
      {
        return layer;
      }
    }

    return {};
  }
}

// ── naming a coordinate system ──────────────────────────────────────────────

TEST(CrsUrnTest, OneSystemUnderEverySpellingServersUse)
{
  const CrsIdentifier plain = parseCrsIdentifier(QStringLiteral("EPSG:3857"));
  const CrsIdentifier noVersion =
    parseCrsIdentifier(QStringLiteral("urn:ogc:def:crs:EPSG::3857"));
  const CrsIdentifier twoPart =
    parseCrsIdentifier(QStringLiteral("urn:ogc:def:crs:EPSG:6.18:3857"));
  const CrsIdentifier threePart =
    parseCrsIdentifier(QStringLiteral("urn:ogc:def:crs:EPSG:6.18.3:3857"));
  const CrsIdentifier http = parseCrsIdentifier(
    QStringLiteral("http://www.opengis.net/def/crs/EPSG/0/3857"));

  // The version segment says which edition of the register was consulted,
  // not which system is meant, and it comes in one, two and three parts.
  EXPECT_EQ(plain.toString(), QStringLiteral("EPSG:3857"));
  EXPECT_EQ(noVersion, plain);
  EXPECT_EQ(twoPart, plain);
  EXPECT_EQ(threePart, plain)
    << "a three-part register version was read as the code";
  EXPECT_EQ(http, plain)
    << "the http form's version segment was read as the authority";
}

TEST(CrsUrnTest, Crs84IsRecognisedUnderBothOfItsNames)
{
  // The one geographic system whose axis order is settled by its name: OGC
  // defined CRS84 so there would be a longitude-first spelling of WGS 84.
  EXPECT_TRUE(parseCrsIdentifier(QStringLiteral("urn:ogc:def:crs:OGC:2:84"))
                .isCrs84());
  EXPECT_TRUE(
    parseCrsIdentifier(QStringLiteral("urn:ogc:def:crs:OGC:1.3:CRS84"))
      .isCrs84());
  EXPECT_TRUE(parseCrsIdentifier(QStringLiteral("OGC:CRS84")).isCrs84());

  EXPECT_FALSE(parseCrsIdentifier(QStringLiteral("EPSG:4326")).isCrs84());
}

TEST(CrsUrnTest, SomethingThatNamesNoSystemIsNotOne)
{
  EXPECT_FALSE(parseCrsIdentifier(QString()).isValid());
  EXPECT_FALSE(parseCrsIdentifier(QStringLiteral("nonsense")).isValid());
  EXPECT_TRUE(parseCrsIdentifier(QStringLiteral("EPSG:3857")).isValid());
}

// ── the pyramid ─────────────────────────────────────────────────────────────

TEST(WmtsCapabilitiesTest, TheServiceAndItsPyramidsAreRead)
{
  const WmtsCapabilities capabilities = vienna();

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();
  EXPECT_EQ(capabilities.version, QStringLiteral("1.0.0"));
  EXPECT_FALSE(capabilities.layers.isEmpty());
  EXPECT_FALSE(capabilities.matrixSets.isEmpty());

  const WmtsTileMatrixSet *set =
    capabilities.matrixSet(QStringLiteral("google3857"));

  ASSERT_NE(set, nullptr) << "the matrix set was not found by its identifier";
  EXPECT_EQ(set->matrices.size(), 21);

  // The set names its system with a three-part register version, which is
  // the form that defeats a naive parse.
  EXPECT_EQ(set->crs.toString(), QStringLiteral("EPSG:3857"));
}

TEST(WmtsCapabilitiesTest, ALevelKnowsHowMuchGroundAPixelCovers)
{
  const WmtsCapabilities capabilities = vienna();
  ASSERT_TRUE(capabilities.ok);

  const WmtsTileMatrixSet *set =
    capabilities.matrixSet(QStringLiteral("google3857"));
  ASSERT_NE(set, nullptr);
  ASSERT_FALSE(set->matrices.isEmpty());

  // The standard fixes a pixel at 0.28 mm, which is what makes a scale
  // denominator mean ground resolution. The top level of a web-mercator
  // pyramid is the whole world in one 256-pixel tile: roughly 156 km per
  // pixel.
  const WmtsTileMatrix &top = set->matrices.first();

  EXPECT_NEAR(top.metresPerPixel(), 156543.0, 50.0);
  EXPECT_EQ(top.matrixWidth, 1);
  EXPECT_EQ(top.matrixHeight, 1);
  EXPECT_EQ(top.tileWidth, 256);
}

TEST(WmtsCapabilitiesTest, TheLevelChosenIsNeverCoarserThanAskedFor)
{
  const WmtsCapabilities capabilities = vienna();
  ASSERT_TRUE(capabilities.ok);

  const WmtsTileMatrixSet *set =
    capabilities.matrixSet(QStringLiteral("google3857"));
  ASSERT_NE(set, nullptr);

  // Drawing from tiles coarser than the map's own scale is a blurred map,
  // so the level chosen never covers more ground per pixel than the map
  // does — and among the levels that satisfy that, the coarsest wins,
  // because a finer one only fetches tiles the screen cannot show.
  const WmtsTileMatrix *chosen = set->matrixFor(1000.0);

  ASSERT_NE(chosen, nullptr);
  EXPECT_LE(chosen->metresPerPixel(), 1000.0)
    << "the map would be drawn from tiles coarser than itself";

  for (const WmtsTileMatrix &matrix : set->matrices)
  {
    if (matrix.metresPerPixel() <= 1000.0)
    {
      EXPECT_GE(chosen->metresPerPixel(), matrix.metresPerPixel())
        << "a finer level than necessary was chosen";
    }
  }

  // A document lists its levels in whatever order it pleases, and this one
  // happens to run coarse to fine — so the same pyramid shuffled has to
  // give the same answer, or the implementation is reading the order and
  // not the scales.
  WmtsTileMatrixSet reversed = *set;
  std::reverse(reversed.matrices.begin(), reversed.matrices.end());

  const WmtsTileMatrix *fromReversed = reversed.matrixFor(1000.0);

  ASSERT_NE(fromReversed, nullptr);
  EXPECT_EQ(fromReversed->identifier, chosen->identifier)
    << "the level chosen depended on the order the document listed them in";

  // Zoomed in past the bottom of the pyramid, every level is coarser than
  // asked for. The finest one is then the best answer there is — not
  // nothing, which would be a blank map at the deepest zoom.
  const WmtsTileMatrix *finest = set->matrixFor(0.0001);
  ASSERT_NE(finest, nullptr) << "the deepest zoom drew nothing at all";

  for (const WmtsTileMatrix &matrix : set->matrices)
  {
    EXPECT_LE(finest->metresPerPixel(), matrix.metresPerPixel());
  }
}

TEST(WmtsCapabilitiesTest, TheFamiliarWebPyramidIsRecognisedAsOne)
{
  const WmtsCapabilities capabilities = vienna();
  ASSERT_TRUE(capabilities.ok);

  const WmtsTileMatrixSet *set =
    capabilities.matrixSet(QStringLiteral("google3857"));
  ASSERT_NE(set, nullptr);

  // A client that already draws slippy-map tiles can draw this one without
  // knowing what a tile matrix is.
  EXPECT_TRUE(set->isWebMercatorQuad());

  // The answer has to be about the grid's shape and not merely about its
  // coordinate system. Each of these is the same pyramid in the same system
  // with one thing changed, and a client that drew either as slippy-map
  // tiles would ask for tiles that are not there.
  WmtsTileMatrixSet bigTiles = *set;

  for (WmtsTileMatrix &matrix : bigTiles.matrices)
  {
    matrix.tileWidth = 512;
    matrix.tileHeight = 512;
  }

  EXPECT_FALSE(bigTiles.isWebMercatorQuad())
    << "512-pixel tiles were taken for the familiar 256-pixel grid";

  WmtsTileMatrixSet ownOrigin = *set;

  for (WmtsTileMatrix &matrix : ownOrigin.matrices)
  {
    matrix.topLeft = QPointF(0.0, 0.0);
  }

  EXPECT_FALSE(ownOrigin.isWebMercatorQuad())
    << "a pyramid hanging from its own origin was taken for the world grid";
}

// ── a layer, and asking it for a tile ───────────────────────────────────────

TEST(WmtsCapabilitiesTest, ALayerCarriesItsStylesFormatsAndPyramids)
{
  const WmtsCapabilities capabilities = vienna();
  ASSERT_TRUE(capabilities.ok);

  const WmtsLayerInfo layer =
    layerNamed(capabilities, QStringLiteral("geolandbasemap"));

  ASSERT_FALSE(layer.identifier.isEmpty()) << "the layer was not found";
  EXPECT_FALSE(layer.title.isEmpty());
  EXPECT_TRUE(layer.formats.contains(QStringLiteral("image/png")));
  EXPECT_FALSE(layer.defaultStyle.isEmpty())
    << "no default style, so a request cannot be built without being told one";
  EXPECT_TRUE(layer.tileMatrixSetIds.contains(QStringLiteral("google3857")));

  // Longitude first: ows:WGS84BoundingBox is CRS84 by definition.
  EXPECT_GT(layer.geographicBounds.width(), 0.0);
  EXPECT_NEAR(layer.geographicBounds.left(), 8.8, 2.0);
  EXPECT_NEAR(layer.geographicBounds.right(), 17.5, 2.0);
}

TEST(WmtsCapabilitiesTest, APublishedTemplateIsUsedRatherThanAQuery)
{
  const WmtsCapabilities capabilities = vienna();
  ASSERT_TRUE(capabilities.ok);

  const WmtsLayerInfo layer =
    layerNamed(capabilities, QStringLiteral("geolandbasemap"));
  ASSERT_FALSE(layer.resourceTemplates.isEmpty())
    << "the RESTful templates were not read at all";

  const QString url = buildWmtsTileUrl(capabilities, layer,
                                       QStringLiteral("google3857"),
                                       QStringLiteral("10"), 357, 558);

  // This server publishes both, and the implementation this replaces emits
  // only a query — which cannot reach a server that publishes templates
  // alone, and here ignores the pool of hosts the templates spread load
  // across.
  EXPECT_FALSE(url.contains(QStringLiteral("REQUEST=GetTile")))
    << "a query was built although the server published a template: "
    << url.toStdString();
  EXPECT_TRUE(url.endsWith(QStringLiteral("/10/357/558.png")))
    << url.toStdString();
  EXPECT_FALSE(url.contains(QStringLiteral("{TileMatrix}")))
    << "a placeholder was left unfilled: " << url.toStdString();
  EXPECT_FALSE(url.contains(QStringLiteral("{Style}")))
    << "a placeholder was left unfilled: " << url.toStdString();
}

TEST(WmtsCapabilitiesTest, AQueryIsBuiltWhenNoTemplateIsPublished)
{
  const WmtsCapabilities capabilities = vienna();
  ASSERT_TRUE(capabilities.ok);

  WmtsLayerInfo layer =
    layerNamed(capabilities, QStringLiteral("geolandbasemap"));
  ASSERT_FALSE(layer.identifier.isEmpty());

  // The same layer as the server would describe it if it offered only the
  // query endpoint, which many do.
  layer.resourceTemplates.clear();

  const QString url = buildWmtsTileUrl(capabilities, layer,
                                       QStringLiteral("google3857"),
                                       QStringLiteral("10"), 357, 558);

  ASSERT_FALSE(url.isEmpty()) << "no request could be built at all";
  EXPECT_TRUE(url.contains(QStringLiteral("SERVICE=WMTS")));
  EXPECT_TRUE(url.contains(QStringLiteral("REQUEST=GetTile")));
  EXPECT_TRUE(url.contains(QStringLiteral("TILEMATRIXSET=google3857")));
  EXPECT_TRUE(url.contains(QStringLiteral("TILEMATRIX=10")));
  EXPECT_TRUE(url.contains(QStringLiteral("TILEROW=357")));
  EXPECT_TRUE(url.contains(QStringLiteral("TILECOL=558")));
  EXPECT_TRUE(url.contains(QStringLiteral("LAYER=geolandbasemap")));
}

// ── when the server refuses ─────────────────────────────────────────────────

TEST(WmtsCapabilitiesTest, AnExceptionReportIsAFailureNotAnEmptyService)
{
  const QByteArray body =
    "<?xml version=\"1.0\"?>"
    "<ows:ExceptionReport xmlns:ows=\"http://www.opengis.net/ows/1.1\">"
    "<ows:Exception exceptionCode=\"InvalidParameterValue\">"
    "<ows:ExceptionText>Unknown TileMatrixSet</ows:ExceptionText>"
    "</ows:Exception></ows:ExceptionReport>";

  const WmtsCapabilities capabilities = parseWmtsCapabilities(body);

  EXPECT_FALSE(capabilities.ok);
  EXPECT_TRUE(capabilities.message.contains(QStringLiteral("TileMatrixSet")))
    << capabilities.message.toStdString();

  const WmtsCapabilities html =
    parseWmtsCapabilities("<html><body>Not found</body></html>");

  EXPECT_FALSE(html.ok);
  EXPECT_FALSE(html.message.isEmpty());
}
