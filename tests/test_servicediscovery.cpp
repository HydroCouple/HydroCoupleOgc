// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   test_servicediscovery.cpp
 * \brief  Working out what a pasted URL is.
 */

#include "hydrocoupleogc/servicediscovery.h"

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

  QString parameter(const QString &url, const QString &name)
  {
    return QUrlQuery(QUrl(url).query()).queryItemValue(name);
  }

  int occurrences(const QString &url, const QString &name)
  {
    int count = 0;

    for (const QPair<QString, QString> &item :
         QUrlQuery(QUrl(url).query()).queryItems())
    {
      if (item.first.compare(name, Qt::CaseInsensitive) == 0)
      {
        ++count;
      }
    }

    return count;
  }
}

TEST(ServiceDiscoveryTest, AnEndpointIsAskedWhatItCanDoInItsOwnDialect)
{
  const QString wms = buildCapabilitiesUrl(
    QStringLiteral("https://maps.example.org/wms"), ServiceKind::Wms);
  const QString wmts = buildCapabilitiesUrl(
    QStringLiteral("https://maps.example.org/wmts"), ServiceKind::Wmts);

  EXPECT_EQ(parameter(wms, QStringLiteral("SERVICE")), QStringLiteral("WMS"));
  EXPECT_EQ(parameter(wms, QStringLiteral("REQUEST")),
            QStringLiteral("GetCapabilities"));
  EXPECT_EQ(parameter(wms, QStringLiteral("VERSION")),
            QStringLiteral("1.3.0"));

  EXPECT_EQ(parameter(wmts, QStringLiteral("SERVICE")),
            QStringLiteral("WMTS"));
  EXPECT_EQ(parameter(wmts, QStringLiteral("VERSION")),
            QStringLiteral("1.0.0"));
}

TEST(ServiceDiscoveryTest, WhatTheAddressAlreadyCarriesIsKept)
{
  // A MapServer address is a mapfile plus a request, and the mapfile is
  // the half that says which service this even is.
  const QString url = buildCapabilitiesUrl(
    QStringLiteral("https://maps.example.org/cgi-bin/mapserv?map=/data/x.map"),
    ServiceKind::Wms);

  EXPECT_EQ(parameter(url, QStringLiteral("map")),
            QStringLiteral("/data/x.map"));
  EXPECT_EQ(parameter(url, QStringLiteral("SERVICE")), QStringLiteral("WMS"));
}

TEST(ServiceDiscoveryTest, AWholeRequestPastedInIsNotAskedTwice)
{
  // As often as not what gets pasted is a complete GetCapabilities copied
  // out of a browser — sometimes for the other service, or an older
  // version. A query carrying two SERVICE parameters is answered by
  // neither server the user might have meant.
  const QString url = buildCapabilitiesUrl(
    QStringLiteral("https://maps.example.org/wms?SERVICE=WMTS&request=GetTile"
                   "&Version=1.1.1&map=/data/x.map"),
    ServiceKind::Wms);

  EXPECT_EQ(occurrences(url, QStringLiteral("SERVICE")), 1)
    << url.toStdString();
  EXPECT_EQ(occurrences(url, QStringLiteral("REQUEST")), 1)
    << url.toStdString();
  EXPECT_EQ(occurrences(url, QStringLiteral("VERSION")), 1)
    << url.toStdString();

  EXPECT_EQ(parameter(url, QStringLiteral("SERVICE")), QStringLiteral("WMS"));
  EXPECT_EQ(parameter(url, QStringLiteral("REQUEST")),
            QStringLiteral("GetCapabilities"));
  EXPECT_EQ(parameter(url, QStringLiteral("map")),
            QStringLiteral("/data/x.map"));
}

TEST(ServiceDiscoveryTest, SomethingThatIsNotAnAddressIsNotAsked)
{
  EXPECT_TRUE(buildCapabilitiesUrl(QString(), ServiceKind::Wms).isEmpty());
  EXPECT_TRUE(buildCapabilitiesUrl(QStringLiteral("   "), ServiceKind::Wms)
                .isEmpty());
  EXPECT_TRUE(buildCapabilitiesUrl(QStringLiteral("not a url"),
                                   ServiceKind::Wms)
                .isEmpty());
  EXPECT_TRUE(buildCapabilitiesUrl(QStringLiteral("https://maps.example.org"),
                                   ServiceKind::Unknown)
                .isEmpty());
}

TEST(ServiceDiscoveryTest, TheServiceSaysWhichItIsByWhatItCallsItsRoot)
{
  // So the user does not have to say. Both spellings of a WMS root, and
  // the WMTS one, which is just "Capabilities".
  EXPECT_EQ(detectServiceKind(fixture(QStringLiteral("wms-1.3.0-nested.xml"))),
            ServiceKind::Wms);
  EXPECT_EQ(detectServiceKind(fixture(QStringLiteral("wms-1.1.1-flat.xml"))),
            ServiceKind::Wms);
  EXPECT_EQ(
    detectServiceKind(fixture(QStringLiteral("wmts-1.0.0-resourceurl.xml"))),
    ServiceKind::Wmts);
}

TEST(ServiceDiscoveryTest, AServiceInsideAnEnvelopeIsStillThatService)
{
  // WMTS may be encoded in SOAP, which puts the same document one element
  // down. The parsers here read it either way, so the detection has to
  // agree with them.
  EXPECT_EQ(detectServiceKind(
              "<?xml version=\"1.0\"?>"
              "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/"
              "soap-envelope\"><soap:Body>"
              "<Capabilities xmlns=\"http://www.opengis.net/wmts/1.0\" "
              "version=\"1.0.0\"/>"
              "</soap:Body></soap:Envelope>"),
            ServiceKind::Wmts);
}

TEST(ServiceDiscoveryTest, AnythingElseIsNotAServiceAtAll)
{
  // An exception report has a root of its own, and a server behind a
  // captive portal answers HTML.
  EXPECT_EQ(detectServiceKind(
              "<?xml version=\"1.0\"?><ows:ExceptionReport "
              "xmlns:ows=\"http://www.opengis.net/ows/1.1\">"
              "<ows:Exception><ows:ExceptionText>No"
              "</ows:ExceptionText></ows:Exception></ows:ExceptionReport>"),
            ServiceKind::Unknown);

  EXPECT_EQ(detectServiceKind("<html><body>Sign in</body></html>"),
            ServiceKind::Unknown);
  EXPECT_EQ(detectServiceKind(QByteArray()), ServiceKind::Unknown);
  EXPECT_EQ(detectServiceKind("{\"not\": \"xml\"}"), ServiceKind::Unknown);
}


TEST(ServiceDiscovery, aCoverageServiceIsNotMistakenForATileService)
{
  // Both call their root element Capabilities and only the namespace
  // separates them, so deciding on the local name alone handed every WCS to
  // the WMTS parser -- which read it as a tile service with no layers, and
  // reported an address that works as one offering nothing.
  EXPECT_EQ(detectServiceKind(
              fixture(QStringLiteral("wcs-2.0.1-pdok-ahn.xml"))),
            ServiceKind::Wcs);

  EXPECT_EQ(detectServiceKind(
              fixture(QStringLiteral("wmts-1.0.0-resourceurl.xml"))),
            ServiceKind::Wmts);
}

TEST(ServiceDiscovery, theOlderCoverageProtocolNamesItselfPlainly)
{
  EXPECT_EQ(detectServiceKind(
              fixture(QStringLiteral("wcs-1.0.0-pdok-ahn.xml"))),
            ServiceKind::Wcs);
}

TEST(ServiceDiscovery, aCoverageServiceIsAskedInItsOwnTerms)
{
  const QString url = buildCapabilitiesUrl(
    QStringLiteral("https://example.org/wcs"), ServiceKind::Wcs);

  EXPECT_TRUE(url.contains(QStringLiteral("SERVICE=WCS"))) << url.toStdString();
  EXPECT_TRUE(url.contains(QStringLiteral("VERSION=2.0.1")))
    << url.toStdString();
}
