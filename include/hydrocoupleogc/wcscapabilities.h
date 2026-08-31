// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   wcscapabilities.h
 * \author Caleb Buahin
 * \brief  What a WCS server offers, and what one of its coverages is.
 *
 * A WCS answers with values rather than with a picture of them, which is
 * what makes it the service that can put elevations on mesh vertices
 * instead of merely a hillshade behind them.
 *
 * It is also the one service here where the capabilities document is not
 * enough. WMS, WMTS and WFS all publish, per layer, where it is and what
 * it can be asked for in. A WCS 2.0 CoverageSummary is permitted to carry
 * nothing but an identifier, and real ones do: PDOK's national elevation
 * service publishes exactly `<CoverageId>` and `<CoverageSubtype>` per
 * coverage and not one word about extent, system or bands. Everything a
 * request needs comes from DescribeCoverage.
 *
 * So there are two documents here, not one, and the second is not
 * optional.
 */

#ifndef HYDROCOUPLEOGC_WCSCAPABILITIES_H
#define HYDROCOUPLEOGC_WCSCAPABILITIES_H

#include "hydrocoupleogc/export.h"

#include <QByteArray>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace HydroCouple::Ogc
{
  /*!
   * \brief One coverage as the capabilities document lists it.
   *
   * Thin on purpose. At 2.0 a server may fill in nothing but \a identifier,
   * and the fields below are what it happens to volunteer. Nothing that
   * builds a request should read them; read a WcsCoverageDescription.
   *
   * Curiously the older protocol is the richer one: a 1.0.0
   * CoverageOfferingBrief carries a name, a label, a description and a
   * lon/lat envelope, so for 1.0 servers this is often complete.
   */
  struct HYDROCOUPLEOGC_EXPORT WcsCoverageSummary
  {
      //! What GetCoverage asks for. The only field always present.
      QString identifier;

      QString title;
      QString abstractText;

      //! Longitude/latitude degrees, when the summary gives them.
      QRectF geographicBounds;

      //! Systems the summary names, as the server spells them.
      QStringList supportedCrs;

      //! Formats the summary names, as the server spells them.
      QStringList supportedFormats;
  };

  /*!
   * \brief One band, or field, of a coverage's range.
   */
  struct HYDROCOUPLEOGC_EXPORT WcsRangeField
  {
      //! What RANGESUBSET names, e.g. "hoogte", "Red", "temperature".
      QString name;

      QString description;

      //! Unit of measure code, e.g. "m". Empty when unstated.
      QString uom;
  };

  /*!
   * \brief What DescribeCoverage says one coverage actually is.
   *
   * The load-bearing document. Three things live only here, and a request
   * built without them is a guess:
   *
   *  - the envelope, and the system it is written in;
   *  - the **axis labels**, which WCS 2.0 subsets by name;
   *  - the range fields, which is the list this library's caller must
   *    choose a band from.
   */
  struct HYDROCOUPLEOGC_EXPORT WcsCoverageDescription
  {
      bool ok = false;
      QString message;

      QString identifier;

      /*!
       * \brief The system the envelope below is written in, as the server
       *        spells it.
       *
       * Often the long URI form,
       * "http://www.opengis.net/def/crs/EPSG/0/28992".
       */
      QString envelopeCrs;

      /*!
       * \brief What the coverage's axes are called, in the envelope's own
       *        order.
       *
       * Not decoration. WCS 2.0 subsets an axis **by name**, and a name
       * that is not the coverage's own is refused: rasdaman answers
       * `InvalidAxisLabel` with HTTP 404. The names vary by coverage, not
       * by protocol -- "x y" for a projected national grid, "Lat Lon" for
       * a geographic one, "ansi Lat Lon" when a time axis leads.
       */
      QStringList axisLabels;

      //! The envelope's corners, in the same axis order as \a axisLabels.
      QList<double> lowerCorner;
      QList<double> upperCorner;

      QList<WcsRangeField> rangeFields;

      /*!
       * \brief Whether this coverage has exactly the two spatial axes this
       *        library can request.
       *
       * A coverage over time or depth needs those axes pinned too, which
       * is a question for the caller and not something to guess.
       */
      [[nodiscard]] bool isTwoDimensional() const;

      /*!
       * \brief The envelope as a rectangle, x horizontal.
       *
       * Ordered by what the axes are, not by where they sit: a coverage
       * whose axes read "Lat Lon" has its northing first, and returning
       * that as a QRectF's x would put the map on its side.
       */
      [[nodiscard]] QRectF boundsAsRect() const;

      /*!
       * \brief Which of \a axisLabels is the easting/longitude one.
       * \returns Its index, or -1 when the axes cannot be told apart.
       */
      [[nodiscard]] int horizontalAxisIndex() const;

      //! Which of \a axisLabels is the northing/latitude one, or -1.
      [[nodiscard]] int verticalAxisIndex() const;
  };

  /*!
   * \brief What a WCS server said about itself.
   */
  struct HYDROCOUPLEOGC_EXPORT WcsCapabilities
  {
      bool ok = false;
      QString message;

      /*!
       * \brief The version the document declares, which need not be the
       *        one that was asked for.
       *
       * A server without the requested version answers in its newest
       * instead of refusing, so this is read from the answer. PDOK's
       * elevation service replies to a VERSION=1.1.2 request with a 2.0.1
       * document.
       */
      QString version;

      QString title;
      QString abstractText;

      QList<WcsCoverageSummary> coverages;

      //! \returns The coverage with \a identifier, or nullptr.
      [[nodiscard]] const WcsCoverageSummary *coverage(
        const QString &identifier) const;
  };

  /*!
   * \brief Reads a WCS GetCapabilities response.
   * \param xml The response body.
   * \returns What the server offers; check \a ok first.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT WcsCapabilities
  parseWcsCapabilities(const QByteArray &xml);

  /*!
   * \brief Reads a WCS 2.0 DescribeCoverage response.
   * \param xml The response body.
   * \returns The first coverage described; check \a ok first.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT WcsCoverageDescription
  parseWcsCoverageDescription(const QByteArray &xml);

  /*!
   * \brief What an OGC exception report says, if that is what this is.
   *
   * Worth asking before concluding anything else about a failed request.
   * A WCS answers a malformed parameter with an exception report under an
   * error status -- rasdaman uses 404 for `InvalidAxisLabel` -- which is
   * indistinguishable from "no such endpoint" unless the body is read. A
   * client that does not read it retries the request as an older protocol
   * version and reports the wrong reason for the wrong failure.
   *
   * \param body The response body.
   * \returns The exception text, or empty when this is not a report.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString
  wcsExceptionText(const QByteArray &body);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_WCSCAPABILITIES_H
