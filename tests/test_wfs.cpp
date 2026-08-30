// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   test_wfs.cpp
 * \brief  Reading a WFS, and asking it for features.
 *
 * The fixture is a real national feature service — the Dutch buildings and
 * addresses register — chosen because it has the two properties that catch
 * a client out: it holds millions of features, so a request without a
 * limit is a request for all of them, and it publishes each collection in
 * a national grid by default with WGS 84 merely among the alternatives.
 */

#include "hydrocoupleogc/servicediscovery.h"
#include "hydrocoupleogc/wfscapabilities.h"
#include "hydrocoupleogc/wfsrequest.h"

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

  WfsCapabilities pdok()
  {
    return parseWfsCapabilities(fixture(QStringLiteral("wfs-2.0.0-pdok.xml")));
  }

  QString parameter(const QString &url, const QString &name)
  {
    return QUrlQuery(QUrl(url).query()).queryItemValue(name);
  }

  bool has(const QString &url, const QString &name)
  {
    return QUrlQuery(QUrl(url).query()).hasQueryItem(name);
  }

  WfsGetFeatureRequest buildingsIn(const QString &crs)
  {
    WfsGetFeatureRequest request;
    request.typeName = QStringLiteral("bag:pand");
    request.crs = crs;
    request.count = 500;

    return request;
  }
}

// ── what the service holds ──────────────────────────────────────────────────

TEST(WfsCapabilitiesTest, TheCollectionsAreReadWithWhatEachCanBeAskedFor)
{
  const WfsCapabilities capabilities = pdok();

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();
  EXPECT_EQ(capabilities.version, QStringLiteral("2.0.0"));
  EXPECT_FALSE(capabilities.title.isEmpty());
  EXPECT_EQ(capabilities.featureTypes.size(), 5);

  const WfsFeatureType *buildings =
    capabilities.featureType(QStringLiteral("bag:pand"));

  ASSERT_NE(buildings, nullptr) << "the collection was not found by name";
  EXPECT_FALSE(buildings->title.isEmpty());

  // What a collection is published in is the collection's business, not the
  // service's: this one defaults to the national grid and offers WGS 84
  // among the alternatives.
  EXPECT_EQ(buildings->defaultCrs,
            QStringLiteral("urn:ogc:def:crs:EPSG::28992"));
  EXPECT_FALSE(buildings->otherCrs.isEmpty());
  EXPECT_GT(buildings->allCrs().size(), buildings->otherCrs.size());

  // Longitude first: ows:WGS84BoundingBox is CRS84 by definition.
  EXPECT_NEAR(buildings->geographicBounds.left(), 2.53, 0.1);
  EXPECT_NEAR(buildings->geographicBounds.right(), 7.37, 0.1);
  EXPECT_GT(buildings->geographicBounds.height(), 0.0);
}

TEST(WfsCapabilitiesTest, ASystemIsRecognisedUnderTheSpellingTheServerUses)
{
  const WfsCapabilities capabilities = pdok();
  ASSERT_TRUE(capabilities.ok);

  const WfsFeatureType *buildings =
    capabilities.featureType(QStringLiteral("bag:pand"));
  ASSERT_NE(buildings, nullptr);

  // Asked for by authority and code; answered in the server's own words,
  // because that is what the request has to carry back.
  EXPECT_EQ(buildings->spellingOf(QStringLiteral("EPSG:4326")),
            QStringLiteral("urn:ogc:def:crs:EPSG::4326"));
  EXPECT_EQ(buildings->spellingOf(QStringLiteral("urn:ogc:def:crs:EPSG::3857")),
            QStringLiteral("urn:ogc:def:crs:EPSG::3857"));

  // And a system it does not publish is not quietly substituted.
  EXPECT_TRUE(buildings->spellingOf(QStringLiteral("EPSG:2913")).isEmpty());
}

TEST(WfsCapabilitiesTest, GeoJsonIsPreferredToGmlWhereThereIsAChoice)
{
  const WfsCapabilities capabilities = pdok();
  ASSERT_TRUE(capabilities.ok);

  const WfsFeatureType *buildings =
    capabilities.featureType(QStringLiteral("bag:pand"));
  ASSERT_NE(buildings, nullptr);
  ASSERT_FALSE(buildings->outputFormats.isEmpty());

  // GML is listed first by this server, and reading it needs a schema
  // fetch and a driver where GeoJSON needs neither.
  const QString chosen =
    preferredOutputFormat(*buildings, capabilities.outputFormats);

  EXPECT_TRUE(isGeoJsonFormat(chosen)) << chosen.toStdString();
  EXPECT_TRUE(buildings->outputFormats.contains(chosen))
    << "a format the collection never advertised was chosen: "
    << chosen.toStdString();

  // A collection offering only GML still gets an answer, because GML is
  // readable — just not for free.
  WfsFeatureType gmlOnly;
  gmlOnly.outputFormats =
    QStringList{QStringLiteral("text/xml; subtype=gml/3.2.1")};

  EXPECT_FALSE(preferredOutputFormat(gmlOnly, {}).isEmpty());
  EXPECT_FALSE(isGeoJsonFormat(preferredOutputFormat(gmlOnly, {})));

  // And a collection offering nothing this program can read says so
  // rather than picking something at random.
  WfsFeatureType shapefileOnly;
  shapefileOnly.outputFormats = QStringList{QStringLiteral("SHAPE-ZIP")};

  EXPECT_TRUE(preferredOutputFormat(shapefileOnly, {}).isEmpty());

  // Plenty of servers list formats only once, for the service, and leave
  // their collections silent — so the service-level list is what those
  // fall back on, and it has to be the formats of GetFeature and nothing
  // else. This document carries eighteen allowed values across five
  // parameters; five of them are these.
  EXPECT_EQ(capabilities.outputFormats.size(), 5)
    << "the allowed values of some other parameter were read as formats";

  for (const QString &format : capabilities.outputFormats)
  {
    EXPECT_TRUE(format.contains(QLatin1Char('/')))
      << format.toStdString() << " is not a media type";
  }

  EXPECT_FALSE(capabilities.outputFormats.contains(QStringLiteral("2.0.0")))
    << "the versions this server accepts were read as output formats";

  WfsFeatureType silent;
  silent.name = QStringLiteral("bag:pand");

  EXPECT_TRUE(
    isGeoJsonFormat(preferredOutputFormat(silent, capabilities.outputFormats)))
    << "a collection that lists no formats of its own got none";
}

TEST(WfsCapabilitiesTest, AnExceptionReportIsAFailureNotAnEmptyService)
{
  const WfsCapabilities refused = parseWfsCapabilities(
    "<?xml version=\"1.0\"?><ows:ExceptionReport "
    "xmlns:ows=\"http://www.opengis.net/ows/1.1\"><ows:Exception>"
    "<ows:ExceptionText>Unknown typeName</ows:ExceptionText>"
    "</ows:Exception></ows:ExceptionReport>");

  EXPECT_FALSE(refused.ok);
  EXPECT_TRUE(refused.message.contains(QStringLiteral("typeName")))
    << refused.message.toStdString();

  const WfsCapabilities html =
    parseWfsCapabilities("<html><body>Sign in</body></html>");

  EXPECT_FALSE(html.ok);
  EXPECT_FALSE(html.message.isEmpty());
  EXPECT_TRUE(parseWfsCapabilities(QByteArray()).message.contains(
    QStringLiteral("empty")));
}

// ── asking for features ─────────────────────────────────────────────────────

TEST(WfsRequestTest, ARequestNamesOneCollectionAndLimitsWhatItAsksFor)
{
  const WfsCapabilities capabilities = pdok();
  ASSERT_TRUE(capabilities.ok);

  const QString url = buildGetFeatureUrl(
    capabilities, buildingsIn(QStringLiteral("urn:ogc:def:crs:EPSG::4326")));

  ASSERT_FALSE(url.isEmpty());
  EXPECT_EQ(parameter(url, QStringLiteral("SERVICE")), QStringLiteral("WFS"));
  EXPECT_EQ(parameter(url, QStringLiteral("REQUEST")),
            QStringLiteral("GetFeature"));
  EXPECT_EQ(parameter(url, QStringLiteral("VERSION")),
            QStringLiteral("2.0.0"));
  EXPECT_EQ(parameter(url, QStringLiteral("SRSNAME")),
            QStringLiteral("urn:ogc:def:crs:EPSG::4326"));

  // This service holds every building in the Netherlands. A request that
  // names no limit asks for all of them, and the server will try.
  EXPECT_EQ(parameter(url, QStringLiteral("COUNT")), QStringLiteral("500"));

  // Where the server said GetFeature requests go, which need not be where
  // the capabilities came from.
  EXPECT_TRUE(url.startsWith(QStringLiteral("https://service.pdok.nl")))
    << url.toStdString();
}

TEST(WfsRequestTest, TheRequestIsSpelledTheWayTheDocumentsVersionSpellsIt)
{
  WfsCapabilities modern = pdok();
  ASSERT_TRUE(modern.ok);

  WfsCapabilities older = modern;
  older.version = QStringLiteral("1.1.0");

  const QString newUrl =
    buildGetFeatureUrl(modern, buildingsIn(QStringLiteral("EPSG:28992")));
  const QString oldUrl =
    buildGetFeatureUrl(older, buildingsIn(QStringLiteral("EPSG:28992")));

  // 2.0.0 renamed both of these. Naming the collection wrongly is reported;
  // limiting wrongly is NOT — a 1.1.0 server sent COUNT ignores it and
  // returns every feature it has.
  EXPECT_TRUE(has(newUrl, QStringLiteral("TYPENAMES")));
  EXPECT_FALSE(has(newUrl, QStringLiteral("TYPENAME")));
  EXPECT_TRUE(has(newUrl, QStringLiteral("COUNT")));
  EXPECT_FALSE(has(newUrl, QStringLiteral("MAXFEATURES")));

  EXPECT_TRUE(has(oldUrl, QStringLiteral("TYPENAME")));
  EXPECT_FALSE(has(oldUrl, QStringLiteral("TYPENAMES")));
  EXPECT_TRUE(has(oldUrl, QStringLiteral("MAXFEATURES")))
    << "a 1.1.0 server would have returned every feature it holds";
  EXPECT_FALSE(has(oldUrl, QStringLiteral("COUNT")));
}

TEST(WfsRequestTest, TheGroundAskedForIsWrittenInTheOrderTheServerReadsIt)
{
  const WfsCapabilities capabilities = pdok();
  ASSERT_TRUE(capabilities.ok);

  // A catchment somewhere over the Netherlands, in degrees.
  WfsGetFeatureRequest geographic =
    buildingsIn(QStringLiteral("urn:ogc:def:crs:EPSG::4326"));
  geographic.extent = QRectF(QPointF(4.0, 51.0), QPointF(6.0, 53.0));

  const QStringList box =
    parameter(buildGetFeatureUrl(capabilities, geographic),
              QStringLiteral("BBOX"))
      .split(QLatin1Char(','));

  // From 1.1.0 on the box is in the authority's order, which for EPSG:4326
  // is latitude first — and 2.0.0 lets the box name its own system so the
  // server need not guess what was meant.
  ASSERT_EQ(box.size(), 5) << "the box did not name the system it is in";
  EXPECT_NEAR(box.at(0).toDouble(), 51.0, 0.001)
    << "latitude was not written first for an EPSG:4326 box";
  EXPECT_NEAR(box.at(1).toDouble(), 4.0, 0.001);
  EXPECT_EQ(box.at(4), QStringLiteral("urn:ogc:def:crs:EPSG::4326"));

  // The same ground in the projected national grid, which is easting
  // first however geographic its neighbours are.
  WfsGetFeatureRequest projected =
    buildingsIn(QStringLiteral("urn:ogc:def:crs:EPSG::28992"));
  projected.extent = QRectF(QPointF(80000.0, 440000.0),
                            QPointF(120000.0, 460000.0));

  const QStringList grid =
    parameter(buildGetFeatureUrl(capabilities, projected),
              QStringLiteral("BBOX"))
      .split(QLatin1Char(','));

  ASSERT_EQ(grid.size(), 5);
  EXPECT_NEAR(grid.at(0).toDouble(), 80000.0, 1.0)
    << "a projected box was swapped";
  EXPECT_NEAR(grid.at(1).toDouble(), 440000.0, 1.0);
}

TEST(WfsRequestTest, TheNextPageIsAskedForOnlyWhereThereIsPaging)
{
  WfsCapabilities modern = pdok();
  ASSERT_TRUE(modern.ok);

  WfsCapabilities older = modern;
  older.version = QStringLiteral("1.1.0");

  WfsGetFeatureRequest second = buildingsIn(QStringLiteral("EPSG:4326"));
  second.startIndex = 500;

  EXPECT_EQ(parameter(buildGetFeatureUrl(modern, second),
                      QStringLiteral("STARTINDEX")),
            QStringLiteral("500"));

  // Paging arrived with 2.0.0. Sending it to a 1.1.0 server does not page
  // it; it just gets ignored, and the same first page comes back forever.
  EXPECT_FALSE(has(buildGetFeatureUrl(older, second),
                   QStringLiteral("STARTINDEX")));

  // The first page names no index at all.
  EXPECT_FALSE(has(buildGetFeatureUrl(modern,
                                      buildingsIn(QStringLiteral("EPSG:4326"))),
                   QStringLiteral("STARTINDEX")));
}

TEST(WfsRequestTest, ARequestThatCouldNotBeAnsweredIsNotBuilt)
{
  const WfsCapabilities capabilities = pdok();
  ASSERT_TRUE(capabilities.ok);

  WfsGetFeatureRequest noType = buildingsIn(QStringLiteral("EPSG:4326"));
  noType.typeName.clear();

  WfsGetFeatureRequest noLimit = buildingsIn(QStringLiteral("EPSG:4326"));
  noLimit.count = 0;

  EXPECT_TRUE(buildGetFeatureUrl(capabilities, noType).isEmpty());
  EXPECT_TRUE(buildGetFeatureUrl(capabilities, noLimit).isEmpty())
    << "an unlimited request against a national service was built";

  WfsCapabilities silent = capabilities;
  silent.getFeatureUrl.clear();

  EXPECT_TRUE(
    buildGetFeatureUrl(silent, buildingsIn(QStringLiteral("EPSG:4326")))
      .isEmpty());
}

// ── and it is recognised as a service at all ────────────────────────────────

TEST(WfsRequestTest, AFeatureServiceIsRecognisedFromItsAddressAndItsAnswer)
{
  const QString url = buildCapabilitiesUrl(
    QStringLiteral("https://service.pdok.nl/kadaster/bag/wfs/v2_0"),
    ServiceKind::Wfs);

  EXPECT_EQ(parameter(url, QStringLiteral("SERVICE")), QStringLiteral("WFS"));
  EXPECT_EQ(parameter(url, QStringLiteral("VERSION")),
            QStringLiteral("2.0.0"));

  EXPECT_EQ(detectServiceKind(fixture(QStringLiteral("wfs-2.0.0-pdok.xml"))),
            ServiceKind::Wfs);
}
