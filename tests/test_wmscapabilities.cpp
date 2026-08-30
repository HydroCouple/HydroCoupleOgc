// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   test_wmscapabilities.cpp
 * \brief  Reading what a WMS server says it can do.
 *
 * The fixtures are real GetCapabilities responses, saved from a public
 * server rather than written by hand, because the failures worth catching
 * here are the ones real servers actually produce. Both are the same
 * service asked in two versions, so the pair also pins the differences
 * between them.
 *
 * Nothing here touches the network. The parser is a function from bytes to
 * a struct, which is the whole reason it is separate from the layer that
 * draws the imagery.
 */

#include "hydrocoupleogc/wmscapabilities.h"

#include <gtest/gtest.h>

#include <QFile>

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

  //! \returns The layer named \a name, or a default-constructed one.
  WmsLayerInfo layerNamed(const WmsCapabilities &capabilities,
                          const QString &name)
  {
    for (const WmsLayerInfo &layer : capabilities.layers)
    {
      if (layer.name == name)
      {
        return layer;
      }
    }

    return {};
  }
}

// ── the tree, and what a layer takes from the layers above it ───────────────

TEST(WmsCapabilitiesTest, ALayerIsDrawableInTheSystemsItsParentDeclared)
{
  const WmsCapabilities capabilities =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();

  // This is the whole point of the parser, and it is not a contrived case:
  // on this server — a commonly used OSM WMS — the unnamed root layer
  // declares twenty-four coordinate systems and every drawable layer
  // beneath it declares none of its own. Read flat, as the implementation
  // this was ported from reads it, every one of these layers comes out able
  // to be drawn in nothing at all, and the CRS chooser is empty.
  const WmsLayerInfo osm = layerNamed(capabilities, QStringLiteral("OSM-WMS"));

  ASSERT_FALSE(osm.title.isEmpty()) << "the layer was not found at all";
  EXPECT_GT(osm.crsIdentifiers.size(), 20)
    << "the layer inherited none of its parent's coordinate systems";
  EXPECT_TRUE(osm.crsIdentifiers.contains(QStringLiteral("EPSG:3857")));
  EXPECT_TRUE(osm.crsIdentifiers.contains(QStringLiteral("EPSG:4326")));
}

TEST(WmsCapabilitiesTest, AGroupLayerIsKeptButCannotBeAskedFor)
{
  const WmsCapabilities capabilities =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();

  // The root here has no Name, so nothing can ask a GetMap for it — but it
  // is the heading the server published its tree under, and dropping it
  // silently (as the implementation this replaces does) changes the shape
  // of what the user is shown.
  const int requestable = capabilities.requestableLayers().size();

  EXPECT_LT(requestable, capabilities.layers.size())
    << "the unnamed group layer was dropped rather than kept";
  EXPECT_GT(requestable, 0);

  for (const WmsLayerInfo &layer : capabilities.requestableLayers())
  {
    EXPECT_FALSE(layer.name.isEmpty());
  }
}

TEST(WmsCapabilitiesTest, AParentIsListedBeforeTheLayersInsideIt)
{
  const WmsCapabilities capabilities =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();
  ASSERT_GT(capabilities.layers.size(), 1);

  // The order is the order a tree view would draw, so a caller can render
  // the list without rebuilding the hierarchy. The root is unnamed and
  // comes first.
  EXPECT_TRUE(capabilities.layers.first().name.isEmpty())
    << "the children were listed ahead of the group they belong to";
}

// ── the two versions ────────────────────────────────────────────────────────

TEST(WmsCapabilitiesTest, TheVersionIsReadFromTheAnswerNotAssumed)
{
  const WmsCapabilities thirteen =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));
  const WmsCapabilities eleven =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.1.1-flat.xml")));

  ASSERT_TRUE(thirteen.ok) << thirteen.message.toStdString();
  ASSERT_TRUE(eleven.ok) << eleven.message.toStdString();

  // A server may answer a 1.3.0 request in 1.1.1, and what follows from the
  // version — how the coordinate system is spelled, which way round a
  // geographic box reads — has to follow the answer.
  EXPECT_EQ(thirteen.version, QStringLiteral("1.3.0"));
  EXPECT_EQ(eleven.version, QStringLiteral("1.1.1"));
}

TEST(WmsCapabilitiesTest, TheOlderVersionSpellsItsSystemsDifferently)
{
  const WmsCapabilities capabilities =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.1.1-flat.xml")));

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();

  // 1.1.1 says SRS where 1.3.0 says CRS, and its root element is named
  // differently too. Both documents describe the same service, so the same
  // layer must come out drawable in the same systems either way.
  const WmsLayerInfo osm = layerNamed(capabilities, QStringLiteral("OSM-WMS"));

  ASSERT_FALSE(osm.title.isEmpty()) << "the layer was not found at all";
  EXPECT_GT(osm.crsIdentifiers.size(), 20)
    << "SRS was not read, so the layer is drawable in nothing";
  EXPECT_TRUE(osm.crsIdentifiers.contains(QStringLiteral("EPSG:3857")));
}

TEST(WmsCapabilitiesTest, TheSameServiceDescribesTheSameLayersInEitherVersion)
{
  const WmsCapabilities thirteen =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));
  const WmsCapabilities eleven =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.1.1-flat.xml")));

  ASSERT_TRUE(thirteen.ok);
  ASSERT_TRUE(eleven.ok);

  QStringList thirteenNames;
  QStringList elevenNames;

  for (const WmsLayerInfo &layer : thirteen.requestableLayers())
  {
    thirteenNames.append(layer.name);
  }

  for (const WmsLayerInfo &layer : eleven.requestableLayers())
  {
    elevenNames.append(layer.name);
  }

  // One gate over both parse paths: the version differences are in the
  // spelling, not in what the service offers.
  EXPECT_EQ(thirteenNames, elevenNames);
}

// ── what the layer covers ───────────────────────────────────────────────────

TEST(WmsCapabilitiesTest, ALayersGroundIsReadAsLongitudeThenLatitude)
{
  const WmsCapabilities thirteen =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));
  const WmsCapabilities eleven =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.1.1-flat.xml")));

  ASSERT_TRUE(thirteen.ok);
  ASSERT_TRUE(eleven.ok);

  const WmsLayerInfo a = layerNamed(thirteen, QStringLiteral("OSM-WMS"));
  const WmsLayerInfo b = layerNamed(eleven, QStringLiteral("OSM-WMS"));

  // 1.3.0 states this in four child elements and 1.1.1 in four attributes,
  // and the two must arrive as the same box. Longitude is the x axis on the
  // way out whatever the document did, so no caller has to ask.
  EXPECT_NEAR(a.geographicBounds.left(), -180.0, 1.0);
  EXPECT_NEAR(a.geographicBounds.right(), 180.0, 1.0);
  EXPECT_NEAR(a.geographicBounds.top(), -90.0, 5.0);
  EXPECT_NEAR(a.geographicBounds.bottom(), 90.0, 5.0);

  EXPECT_NEAR(a.geographicBounds.left(), b.geographicBounds.left(), 1.0);
  EXPECT_NEAR(a.geographicBounds.right(), b.geographicBounds.right(), 1.0);
}

// ── what the service offers ─────────────────────────────────────────────────

TEST(WmsCapabilitiesTest, TheServiceSaysWhatItCanDrawAndWhereToAskIt)
{
  const WmsCapabilities capabilities =
    parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();

  EXPECT_FALSE(capabilities.title.isEmpty());
  EXPECT_FALSE(capabilities.imageFormats.isEmpty())
    << "no image format was read, so a GetMap cannot name one";
  EXPECT_TRUE(capabilities.imageFormats.contains(QStringLiteral("image/png")));

  // Where GetMap goes need not be where the capabilities came from, and a
  // caller that assumed otherwise would ask the wrong URL on every server
  // that separates them.
  EXPECT_TRUE(capabilities.getMapUrl.startsWith(QStringLiteral("http")))
    << capabilities.getMapUrl.toStdString();

  // Formats are image types only: a capabilities document also advertises
  // text/xml for its exceptions and for GetFeatureInfo.
  for (const QString &format : capabilities.imageFormats)
  {
    EXPECT_TRUE(format.startsWith(QStringLiteral("image/"))) << format.toStdString();
  }
}

// ── when the server refuses ─────────────────────────────────────────────────

TEST(WmsCapabilitiesTest, AnExceptionReportIsAFailureNotAnEmptyService)
{
  // Servers send these with HTTP 200, and often with an image content type,
  // so the body is the only place the refusal is stated. Read as a
  // capabilities document it looks like a service offering no layers, which
  // is indistinguishable from a working server with nothing published.
  const QByteArray body =
    "<?xml version=\"1.0\"?>"
    "<ServiceExceptionReport version=\"1.3.0\">"
    "<ServiceException code=\"InvalidFormat\">"
    "Parameter FORMAT has an invalid value."
    "</ServiceException></ServiceExceptionReport>";

  const WmsCapabilities capabilities = parseWmsCapabilities(body);

  EXPECT_FALSE(capabilities.ok);
  EXPECT_TRUE(capabilities.message.contains(QStringLiteral("FORMAT")))
    << "the server said why and it was not carried out: "
    << capabilities.message.toStdString();
  EXPECT_TRUE(capabilities.layers.isEmpty());
}

TEST(WmsCapabilitiesTest, SomethingThatIsNotCapabilitiesIsRefusedWithAReason)
{
  const WmsCapabilities html =
    parseWmsCapabilities("<html><body>404 Not Found</body></html>");

  EXPECT_FALSE(html.ok);
  EXPECT_FALSE(html.message.isEmpty());

  const WmsCapabilities empty = parseWmsCapabilities({});

  EXPECT_FALSE(empty.ok);
  EXPECT_FALSE(empty.message.isEmpty());
}
