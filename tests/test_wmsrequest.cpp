// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   test_wmsrequest.cpp
 * \brief  Asking a WMS for a picture of somewhere.
 *
 * Both fixtures are real capabilities documents, one 1.3.0 and one 1.1.1,
 * because everything gated here follows from the version the document
 * declares rather than from the version that was asked for.
 */

#include "hydrocoupleogc/wmscapabilities.h"
#include "hydrocoupleogc/wmsrequest.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QUrl>
#include <QUrlQuery>

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

  WmsCapabilities modern()
  {
    return parseWmsCapabilities(fixture(QStringLiteral("wms-1.3.0-nested.xml")));
  }

  WmsCapabilities older()
  {
    return parseWmsCapabilities(fixture(QStringLiteral("wms-1.1.1-flat.xml")));
  }

  //! \returns One query parameter of \a url, by name.
  QString parameter(const QString &url, const QString &name)
  {
    return QUrlQuery(QUrl(url).query()).queryItemValue(name);
  }

  WmsGetMapRequest tileRequest()
  {
    WmsGetMapRequest request;
    request.layers = QStringList{QStringLiteral("osm")};
    request.crs = QStringLiteral("EPSG:3857");
    request.extent =
      QRectF(QPointF(-20037508.34, -20037508.34), QPointF(0.0, 0.0));
    request.format = QStringLiteral("image/png");

    return request;
  }
}

TEST(WmsRequestTest, ARequestNamesTheLayerTheSizeAndTheGround)
{
  const WmsCapabilities capabilities = modern();
  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();

  const QString url = buildGetMapUrl(capabilities, tileRequest());

  ASSERT_FALSE(url.isEmpty());
  EXPECT_EQ(parameter(url, QStringLiteral("SERVICE")), QStringLiteral("WMS"));
  EXPECT_EQ(parameter(url, QStringLiteral("REQUEST")),
            QStringLiteral("GetMap"));
  EXPECT_EQ(parameter(url, QStringLiteral("LAYERS")), QStringLiteral("osm"));
  EXPECT_EQ(parameter(url, QStringLiteral("WIDTH")), QStringLiteral("256"));
  EXPECT_EQ(parameter(url, QStringLiteral("HEIGHT")), QStringLiteral("256"));
  EXPECT_EQ(parameter(url, QStringLiteral("FORMAT")),
            QStringLiteral("image/png"));

  // Drawn over a basemap unless told otherwise.
  EXPECT_EQ(parameter(url, QStringLiteral("TRANSPARENT")),
            QStringLiteral("TRUE"));

  // Servers refuse a GetMap that leaves STYLES out, so it is sent even when
  // every layer is to be drawn in its default style.
  const QUrlQuery query(QUrl(url).query());
  EXPECT_TRUE(query.hasQueryItem(QStringLiteral("STYLES")))
    << "STYLES was omitted rather than sent empty";
}

TEST(WmsRequestTest, TheSystemIsNamedTheWayTheDocumentsVersionNamesIt)
{
  const QString modernUrl = buildGetMapUrl(modern(), tileRequest());
  const QString olderUrl = buildGetMapUrl(older(), tileRequest());

  ASSERT_FALSE(modernUrl.isEmpty());
  ASSERT_FALSE(olderUrl.isEmpty());

  // The parameter was renamed in 1.3.0, and a server given the other
  // spelling answers with an exception rather than a map.
  EXPECT_EQ(parameter(modernUrl, QStringLiteral("CRS")),
            QStringLiteral("EPSG:3857"));
  EXPECT_TRUE(parameter(modernUrl, QStringLiteral("SRS")).isEmpty());

  EXPECT_EQ(parameter(olderUrl, QStringLiteral("SRS")),
            QStringLiteral("EPSG:3857"));
  EXPECT_TRUE(parameter(olderUrl, QStringLiteral("CRS")).isEmpty());

  // And the version sent is the one the server answered in, not the one it
  // was asked in.
  EXPECT_EQ(parameter(modernUrl, QStringLiteral("VERSION")),
            QStringLiteral("1.3.0"));
  EXPECT_EQ(parameter(olderUrl, QStringLiteral("VERSION")),
            QStringLiteral("1.1.1"));
}

TEST(WmsRequestTest, TheBoundingBoxIsWrittenInTheOrderTheAuthorityUses)
{
  // 1.3.0 writes a bounding box in the authority's axis order. For a
  // projected system that is easting first, and for EPSG:4326 it is
  // latitude first — the single most common way a WMS client draws a map
  // that is plausible, sideways, and somewhere else.
  EXPECT_FALSE(wmsAxisOrderIsLatitudeFirst(QStringLiteral("EPSG:3857"),
                                           QStringLiteral("1.3.0")))
    << "a projected system was swapped";
  EXPECT_TRUE(wmsAxisOrderIsLatitudeFirst(QStringLiteral("EPSG:4326"),
                                          QStringLiteral("1.3.0")));
  EXPECT_TRUE(wmsAxisOrderIsLatitudeFirst(
    QStringLiteral("urn:ogc:def:crs:EPSG:6.18.3:4326"),
    QStringLiteral("1.3.0")));

  // CRS84 is WGS 84 written longitude first, which is why it exists.
  EXPECT_FALSE(wmsAxisOrderIsLatitudeFirst(QStringLiteral("CRS:84"),
                                           QStringLiteral("1.3.0")));
  EXPECT_FALSE(wmsAxisOrderIsLatitudeFirst(
    QStringLiteral("urn:ogc:def:crs:OGC:2:84"), QStringLiteral("1.3.0")));

  // Only EPSG's register says which way round its own systems go. ESRI
  // reuses the same numbers and writes longitude first regardless.
  EXPECT_FALSE(wmsAxisOrderIsLatitudeFirst(QStringLiteral("ESRI:4326"),
                                           QStringLiteral("1.3.0")));

  // Nothing is swapped before 1.3.0, whatever the system.
  EXPECT_FALSE(wmsAxisOrderIsLatitudeFirst(QStringLiteral("EPSG:4326"),
                                           QStringLiteral("1.1.1")));
}

TEST(WmsRequestTest, TheGroundAskedForIsTheGroundInTheUrl)
{
  const WmsCapabilities capabilities = modern();
  ASSERT_TRUE(capabilities.ok);

  // The south-west quarter of the Web Mercator world, which is the shape a
  // tiled basemap asks for.
  const QString url = buildGetMapUrl(capabilities, tileRequest());
  const QStringList box =
    parameter(url, QStringLiteral("BBOX")).split(QLatin1Char(','));

  ASSERT_EQ(box.size(), 4);
  EXPECT_NEAR(box.at(0).toDouble(), -20037508.34, 0.01);
  EXPECT_NEAR(box.at(1).toDouble(), -20037508.34, 0.01);
  EXPECT_NEAR(box.at(2).toDouble(), 0.0, 0.01);
  EXPECT_NEAR(box.at(3).toDouble(), 0.0, 0.01);

  // The same ground in a geographic system under 1.3.0, where the two
  // numbers of each corner change places.
  WmsGetMapRequest geographic = tileRequest();
  geographic.crs = QStringLiteral("EPSG:4326");
  geographic.extent = QRectF(QPointF(4.0, 51.0), QPointF(6.0, 53.0));

  const QStringList swapped =
    parameter(buildGetMapUrl(capabilities, geographic),
              QStringLiteral("BBOX"))
      .split(QLatin1Char(','));

  ASSERT_EQ(swapped.size(), 4);
  EXPECT_NEAR(swapped.at(0).toDouble(), 51.0, 0.001)
    << "latitude was not written first for an EPSG:4326 request under 1.3.0";
  EXPECT_NEAR(swapped.at(1).toDouble(), 4.0, 0.001);
  EXPECT_NEAR(swapped.at(2).toDouble(), 53.0, 0.001);
  EXPECT_NEAR(swapped.at(3).toDouble(), 6.0, 0.001);
}

TEST(WmsRequestTest, TheRequestGoesWhereTheServerSaidGetMapRequestsGo)
{
  const WmsCapabilities capabilities = modern();
  ASSERT_TRUE(capabilities.ok);
  ASSERT_FALSE(capabilities.getMapUrl.isEmpty());

  const QString url = buildGetMapUrl(capabilities, tileRequest());

  // Not the address the capabilities were fetched from: a server may serve
  // its maps from somewhere else entirely, and says so.
  EXPECT_TRUE(url.startsWith(capabilities.getMapUrl.left(20)))
    << url.toStdString();

  // A server that publishes an endpoint already carrying parameters keeps
  // them.
  WmsCapabilities withQuery = capabilities;
  withQuery.getMapUrl =
    QStringLiteral("https://maps.example.org/wms?map=/data/roads.map");

  const QString kept = buildGetMapUrl(withQuery, tileRequest());

  EXPECT_EQ(parameter(kept, QStringLiteral("map")),
            QStringLiteral("/data/roads.map"))
    << "the endpoint's own parameters were dropped: " << kept.toStdString();
  EXPECT_EQ(parameter(kept, QStringLiteral("REQUEST")),
            QStringLiteral("GetMap"));
}

TEST(WmsRequestTest, ARequestThatNamesNothingDrawableIsNotBuilt)
{
  const WmsCapabilities capabilities = modern();
  ASSERT_TRUE(capabilities.ok);

  WmsGetMapRequest noLayers = tileRequest();
  noLayers.layers.clear();

  WmsGetMapRequest noCrs = tileRequest();
  noCrs.crs.clear();

  WmsGetMapRequest noSize = tileRequest();
  noSize.widthPixels = 0;

  EXPECT_TRUE(buildGetMapUrl(capabilities, noLayers).isEmpty());
  EXPECT_TRUE(buildGetMapUrl(capabilities, noCrs).isEmpty());
  EXPECT_TRUE(buildGetMapUrl(capabilities, noSize).isEmpty());

  // And a service that never said where its maps come from.
  WmsCapabilities silent = capabilities;
  silent.getMapUrl.clear();

  EXPECT_TRUE(buildGetMapUrl(silent, tileRequest()).isEmpty());
}
