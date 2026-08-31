// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file test_wcs.cpp
 * \brief What a coverage service says, and how it must be asked.
 *
 * The fixtures are real answers from two public services, and between
 * them they carry the whole difficulty of WCS:
 *
 *  - PDOK's Actueel Hoogtebestand Nederland, the Dutch national elevation
 *    model, whose 2.0 capabilities publish an identifier per coverage and
 *    nothing else at all, and whose coverages sit in a projected national
 *    grid with axes called "x" and "y";
 *  - rasdaman's Germany_DTM, in geographic coordinates with axes called
 *    "Lat" and "Lon", latitude written first.
 *
 * A request built for one of those is refused by the other. That is the
 * thing these gates exist to hold.
 */

#include <hydrocoupleogc/wcscapabilities.h>
#include <hydrocoupleogc/wcsrequest.h>

#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QString>

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

  WcsCoverageDescription dutchElevation()
  {
    return parseWcsCoverageDescription(
      fixture(QStringLiteral("wcs-2.0.1-pdok-ahn-describe.xml")));
  }

  WcsCoverageDescription germanTerrain()
  {
    return parseWcsCoverageDescription(
      fixture(QStringLiteral("wcs-2.0.1-rasdaman-germany-describe.xml")));
  }
} // namespace

// ---------------------------------------------------------------------------
// What the capabilities document can and cannot tell you
// ---------------------------------------------------------------------------

TEST(WcsCapabilities, aTwoZeroServiceMayListNothingButItsCoverageNames)
{
  const WcsCapabilities capabilities = parseWcsCapabilities(
    fixture(QStringLiteral("wcs-2.0.1-pdok-ahn.xml")));

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();
  EXPECT_EQ(capabilities.version.toStdString(), "2.0.1");
  ASSERT_EQ(capabilities.coverages.size(), 2);

  const WcsCoverageSummary *dtm =
    capabilities.coverage(QStringLiteral("dtm_05m"));
  ASSERT_NE(dtm, nullptr);

  // The point of the gate: everything a request needs is absent. If this
  // ever starts carrying an extent, DescribeCoverage is still where the
  // axis labels live, and the layer must still ask.
  EXPECT_TRUE(dtm->geographicBounds.isNull());
  EXPECT_TRUE(dtm->supportedCrs.isEmpty());
  EXPECT_TRUE(dtm->title.isEmpty());
}

TEST(WcsCapabilities, theOlderProtocolIsTheRicherListing)
{
  const WcsCapabilities capabilities = parseWcsCapabilities(
    fixture(QStringLiteral("wcs-1.0.0-pdok-ahn.xml")));

  ASSERT_TRUE(capabilities.ok) << capabilities.message.toStdString();
  EXPECT_EQ(capabilities.version.toStdString(), "1.0.0");
  ASSERT_EQ(capabilities.coverages.size(), 2);

  const WcsCoverageSummary *dtm =
    capabilities.coverage(QStringLiteral("dtm_05m"));
  ASSERT_NE(dtm, nullptr);
  EXPECT_EQ(dtm->title.toStdString(), "Digital Terrain Model (DTM) 0.5m");
}

TEST(WcsCapabilities, aOneZeroEnvelopeIsTwoUnnamedCornersInOrder)
{
  const WcsCapabilities capabilities = parseWcsCapabilities(
    fixture(QStringLiteral("wcs-1.0.0-pdok-ahn.xml")));

  ASSERT_TRUE(capabilities.ok);

  const WcsCoverageSummary *dsm =
    capabilities.coverage(QStringLiteral("dsm_05m"));
  ASSERT_NE(dsm, nullptr);

  // <lonLatEnvelope> holds two <gml:pos> siblings rather than a named
  // lower and upper corner. Reading them means matching the LOCAL name --
  // "pos" -- because that is all QXmlStreamReader reports; a comparison
  // against the prefixed spelling matches nothing and leaves every 1.0.0
  // coverage with no extent, which is what the code this was ported from
  // did.
  ASSERT_FALSE(dsm->geographicBounds.isNull());
  EXPECT_NEAR(dsm->geographicBounds.left(), 3.2003, 1e-3);
  EXPECT_NEAR(dsm->geographicBounds.top(), 50.7287, 1e-3);
  EXPECT_NEAR(dsm->geographicBounds.right(), 7.2734, 1e-3);
  EXPECT_NEAR(dsm->geographicBounds.bottom(), 53.5559, 1e-3);
}

TEST(WcsCapabilities, theVersionIsReadFromTheAnswerNotTheQuestion)
{
  // PDOK answers a VERSION=1.1.2 request with a 2.0.1 document rather than
  // refusing, so a client that assumes it got what it asked for writes
  // every subsequent request in a protocol the server never agreed to.
  const WcsCapabilities capabilities = parseWcsCapabilities(
    fixture(QStringLiteral("wcs-2.0.1-pdok-ahn.xml")));

  ASSERT_TRUE(capabilities.ok);
  EXPECT_EQ(capabilities.version.toStdString(), "2.0.1");
}

TEST(WcsCapabilities, somethingThatIsNotACapabilitiesDocumentIsRefused)
{
  const WcsCapabilities capabilities =
    parseWcsCapabilities(QByteArray("<html><body>Sign in</body></html>"));

  EXPECT_FALSE(capabilities.ok);
  EXPECT_FALSE(capabilities.message.isEmpty());
}

// ---------------------------------------------------------------------------
// What DescribeCoverage adds, which is everything that matters
// ---------------------------------------------------------------------------

TEST(WcsCoverageDescription, aProjectedCoverageNamesItsAxesXAndY)
{
  const WcsCoverageDescription description = dutchElevation();

  ASSERT_TRUE(description.ok) << description.message.toStdString();
  EXPECT_EQ(description.identifier.toStdString(), "dtm_05m");
  ASSERT_EQ(description.axisLabels.size(), 2);
  EXPECT_EQ(description.axisLabels.at(0).toStdString(), "x");
  EXPECT_EQ(description.axisLabels.at(1).toStdString(), "y");
  EXPECT_TRUE(description.envelopeCrs.contains(QStringLiteral("28992")));

  const QRectF bounds = description.boundsAsRect();
  EXPECT_NEAR(bounds.left(), 10000.0, 1.0);
  EXPECT_NEAR(bounds.right(), 280000.0, 1.0);
}

TEST(WcsCoverageDescription, aGeographicCoverageMayWriteLatitudeFirst)
{
  const WcsCoverageDescription description = germanTerrain();

  ASSERT_TRUE(description.ok) << description.message.toStdString();
  ASSERT_EQ(description.axisLabels.size(), 2);
  EXPECT_EQ(description.axisLabels.at(0).toStdString(), "Lat");
  EXPECT_EQ(description.axisLabels.at(1).toStdString(), "Lon");

  // The envelope is written latitude first, so the corners as read are
  // (46.99, 4.99) and (56.00, 16.00). A rectangle that took them
  // positionally would put Germany's longitudes on the latitude axis.
  const QRectF bounds = description.boundsAsRect();
  EXPECT_NEAR(bounds.left(), 4.9995, 1e-3);
  EXPECT_NEAR(bounds.right(), 16.0004, 1e-3);
  EXPECT_NEAR(bounds.top(), 46.9995, 1e-3);
  EXPECT_NEAR(bounds.bottom(), 56.0004, 1e-3);
}

TEST(WcsCoverageDescription, theRangeFieldsAreWhatABandCanBeChosenFrom)
{
  const WcsCoverageDescription description = dutchElevation();

  ASSERT_TRUE(description.ok);
  ASSERT_EQ(description.rangeFields.size(), 1);
  EXPECT_EQ(description.rangeFields.at(0).name.toStdString(), "hoogte");
  EXPECT_EQ(description.rangeFields.at(0).uom.toStdString(), "m");
  EXPECT_FALSE(description.rangeFields.at(0).description.isEmpty());
}

TEST(WcsCoverageDescription, anAnswerAboutSeveralCoveragesDescribesTheFirst)
{
  // The element is CoverageDescriptions, plural. Asked about two coverages
  // PDOK answers about both in one document, and both of its elevation
  // coverages happen to carry a band called "hoogte" -- so a parser that
  // reads straight through returns a coverage with the second one's
  // envelope and two identical bands, which is a coverage that does not
  // exist anywhere.
  const WcsCoverageDescription description = parseWcsCoverageDescription(
    fixture(QStringLiteral("wcs-2.0.1-pdok-ahn-describe-two.xml")));

  ASSERT_TRUE(description.ok) << description.message.toStdString();
  EXPECT_EQ(description.identifier.toStdString(), "dtm_05m");
  EXPECT_EQ(description.rangeFields.size(), 1);
}

TEST(WcsCoverageDescription, anExceptionReportIsReadAsOneNotAsADescription)
{
  const QByteArray body =
    fixture(QStringLiteral("wcs-exception-invalid-axis.xml"));

  const WcsCoverageDescription description =
    parseWcsCoverageDescription(body);

  EXPECT_FALSE(description.ok);
  EXPECT_TRUE(description.message.contains(QStringLiteral("InvalidAxisLabel")))
    << description.message.toStdString();
}

TEST(WcsExceptions, aParameterErrorIsNotAMissingEndpoint)
{
  // rasdaman answers a bad axis label with HTTP 404 and this body. Read as
  // a status alone it means "no such service, try an older version"; read
  // as a document it means "you named an axis this coverage has not got".
  // The whole version ladder depends on telling those apart.
  const QString text =
    wcsExceptionText(fixture(QStringLiteral("wcs-exception-invalid-axis.xml")));

  ASSERT_FALSE(text.isEmpty());
  EXPECT_TRUE(text.contains(QStringLiteral("InvalidAxisLabel")));
  EXPECT_TRUE(text.contains(QStringLiteral("Germany_DTM")));
}

TEST(WcsExceptions, anOrdinaryDocumentIsNotAnExceptionReport)
{
  EXPECT_TRUE(
    wcsExceptionText(fixture(QStringLiteral("wcs-2.0.1-pdok-ahn.xml")))
      .isEmpty());
}

// ---------------------------------------------------------------------------
// Building the request
// ---------------------------------------------------------------------------

TEST(WcsRequest, aTwoZeroSubsetIsWrittenAgainstTheCoveragesOwnAxisNames)
{
  WcsGetCoverageRequest request;
  request.coverageId = QStringLiteral("dtm_05m");
  request.extent = QRectF(QPointF(120000, 486000), QPointF(120500, 486500));
  request.size = QSize(512, 512);

  const QString url =
    buildGetCoverageUrl(QStringLiteral("https://example.org/wcs"),
                        QStringLiteral("2.0.1"), request, dutchElevation());

  ASSERT_FALSE(url.isEmpty());
  EXPECT_TRUE(url.contains(QStringLiteral("SUBSET=x(120000.000000,120500.000000)")))
    << url.toStdString();
  EXPECT_TRUE(url.contains(QStringLiteral("SUBSET=y(486000.000000,486500.000000)")))
    << url.toStdString();
  EXPECT_TRUE(url.contains(QStringLiteral("SCALESIZE=x(512),y(512)")))
    << url.toStdString();

  // The names are the coverage's, so the geographic spelling must not
  // appear anywhere in a request for a projected national grid.
  EXPECT_FALSE(url.contains(QStringLiteral("Lon(")));
  EXPECT_FALSE(url.contains(QStringLiteral("Lat(")));
}

TEST(WcsRequest, theSameExtentAgainstADifferentCoverageIsWrittenDifferently)
{
  WcsGetCoverageRequest request;
  request.coverageId = QStringLiteral("Germany_DTM");
  request.extent = QRectF(QPointF(8.0, 50.0), QPointF(8.2, 50.2));

  const QString url =
    buildGetCoverageUrl(QStringLiteral("https://example.org/wcs"),
                        QStringLiteral("2.0.1"), request, germanTerrain());

  ASSERT_FALSE(url.isEmpty());

  // Longitude against the axis called Lon and latitude against Lat, even
  // though the coverage writes latitude first: subsetting is by name, so
  // the order in the URL carries nothing.
  EXPECT_TRUE(url.contains(QStringLiteral("SUBSET=Lon(8.000000,8.200000)")))
    << url.toStdString();
  EXPECT_TRUE(url.contains(QStringLiteral("SUBSET=Lat(50.000000,50.200000)")))
    << url.toStdString();
  EXPECT_FALSE(url.contains(QStringLiteral("SUBSET=x(")));
}

TEST(WcsRequest, aCoverageWhoseAxesAreUnknownIsRefusedRatherThanGuessedAt)
{
  WcsCoverageDescription nameless;
  nameless.ok = true;
  nameless.axisLabels = QStringList{QStringLiteral("ansi"),
                                    QStringLiteral("depth")};

  WcsGetCoverageRequest request;
  request.coverageId = QStringLiteral("something");
  request.extent = QRectF(0, 0, 1, 1);

  // Guessing "Lon"/"Lat" here would produce a request answered with an
  // error status that a version ladder reads as "try an older protocol",
  // so the layer would walk all the way down to 1.0.0 and report the wrong
  // reason for a failure that was never about the version.
  EXPECT_TRUE(buildGetCoverageUrl(QStringLiteral("https://example.org/wcs"),
                                  QStringLiteral("2.0.1"), request, nameless)
                .isEmpty());
}

TEST(WcsRequest, theOneOneFormNamesItsSystemInsideTheBoundingBox)
{
  WcsGetCoverageRequest request;
  request.coverageId = QStringLiteral("dtm_05m");
  request.extent = QRectF(QPointF(120000, 486000), QPointF(120500, 486500));
  request.subsettingCrs = QStringLiteral("EPSG:28992");

  const QString url =
    buildGetCoverageUrl(QStringLiteral("https://example.org/wcs"),
                        QStringLiteral("1.1.2"), request, dutchElevation());

  ASSERT_FALSE(url.isEmpty());
  EXPECT_TRUE(url.contains(QStringLiteral("IDENTIFIER=dtm_05m")));
  EXPECT_TRUE(url.contains(QStringLiteral("urn:ogc:def:crs:EPSG::28992")))
    << url.toStdString();
  EXPECT_FALSE(url.contains(QStringLiteral("SUBSET=")));
}

TEST(WcsRequest, theOneZeroFormIsAPlainBoxAndASize)
{
  WcsGetCoverageRequest request;
  request.coverageId = QStringLiteral("dtm_05m");
  request.extent = QRectF(QPointF(120000, 486000), QPointF(120500, 486500));
  request.subsettingCrs = QStringLiteral("EPSG:28992");
  request.size = QSize(256, 128);

  const QString url =
    buildGetCoverageUrl(QStringLiteral("https://example.org/wcs"),
                        QStringLiteral("1.0.0"), request, dutchElevation());

  ASSERT_FALSE(url.isEmpty());
  EXPECT_TRUE(url.contains(QStringLiteral("COVERAGE=dtm_05m")));
  EXPECT_TRUE(url.contains(QStringLiteral("CRS=EPSG:28992")))
    << url.toStdString();
  EXPECT_TRUE(url.contains(
    QStringLiteral("BBOX=120000.000000,486000.000000,120500.000000,486500.000000")))
    << url.toStdString();
  EXPECT_TRUE(url.contains(QStringLiteral("WIDTH=256")));
  EXPECT_TRUE(url.contains(QStringLiteral("HEIGHT=128")));
}

TEST(WcsRequest, aDescribeCoverageAsksByTheNameItsVersionUses)
{
  EXPECT_TRUE(buildDescribeCoverageUrl(QStringLiteral("https://example.org/wcs"),
                                       QStringLiteral("2.0.1"),
                                       QStringLiteral("dtm_05m"))
                .contains(QStringLiteral("COVERAGEID=dtm_05m")));

  EXPECT_TRUE(buildDescribeCoverageUrl(QStringLiteral("https://example.org/wcs"),
                                       QStringLiteral("1.1.2"),
                                       QStringLiteral("dtm_05m"))
                .contains(QStringLiteral("IDENTIFIERS=dtm_05m")));

  EXPECT_TRUE(buildDescribeCoverageUrl(QStringLiteral("https://example.org/wcs"),
                                       QStringLiteral("1.0.0"),
                                       QStringLiteral("dtm_05m"))
                .contains(QStringLiteral("COVERAGE=dtm_05m")));
}

TEST(WcsRequest, theVersionLadderEndsRatherThanCycling)
{
  EXPECT_EQ(nextWcsVersion(QStringLiteral("2.0.1")).toStdString(), "1.1.2");
  EXPECT_EQ(nextWcsVersion(QStringLiteral("1.1.2")).toStdString(), "1.0.0");
  EXPECT_TRUE(nextWcsVersion(QStringLiteral("1.0.0")).isEmpty());
}
