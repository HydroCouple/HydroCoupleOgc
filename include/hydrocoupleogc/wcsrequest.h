// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   wcsrequest.h
 * \author Caleb Buahin
 * \brief  Asking a WCS for a piece of a coverage.
 *
 * The three protocol versions do not merely spell the same request
 * differently -- they ask differently. 1.0 and 1.1 name a bounding box,
 * which is positional: four numbers whose meaning is the order they are
 * written in. 2.0 subsets each axis **by name**, which is why a request
 * cannot be built from an extent alone and needs the coverage's own
 * description to say what its axes are called.
 */

#ifndef HYDROCOUPLEOGC_WCSREQUEST_H
#define HYDROCOUPLEOGC_WCSREQUEST_H

#include "hydrocoupleogc/export.h"
#include "hydrocoupleogc/wcscapabilities.h"

#include <QRectF>
#include <QSize>
#include <QString>

namespace HydroCouple::Ogc
{
  /*!
   * \brief What to ask a WCS for.
   */
  struct HYDROCOUPLEOGC_EXPORT WcsGetCoverageRequest
  {
      //! The coverage's identifier, as the capabilities document spells it.
      QString coverageId;

      /*!
       * \brief The ground wanted, in \a subsettingCrs.
       *
       * x is the easting or longitude axis whatever the coverage's own
       * axis order is; the builder writes each bound against the axis it
       * belongs to by name.
       */
      QRectF extent;

      /*!
       * \brief The system \a extent is written in, as the server spells it.
       *
       * Empty asks in the coverage's own system, which is always accepted
       * and needs no reprojection on the server.
       */
      QString subsettingCrs;

      //! The system to return the coverage in. Empty leaves it native.
      QString outputCrs;

      QString format = QStringLiteral("image/tiff");

      /*!
       * \brief The size wanted, in pixels.
       *
       * Null asks for the coverage's own resolution over that ground,
       * which for a half-metre national elevation model is a great many
       * more pixels than a screen has.
       */
      QSize size;

      //! Which range fields to return. Empty returns all of them.
      QStringList rangeSubset;
  };

  /*!
   * \brief Builds a GetCoverage URL.
   *
   * \param serviceUrl  Where the service lives.
   * \param version     The protocol version to write, e.g. "2.0.1".
   * \param request     What is wanted.
   * \param description The coverage as DescribeCoverage gave it, whose
   *                    axis labels a 2.0 request is written against.
   *                    Ignored by the 1.x forms, which are positional.
   * \returns The URL, or empty when the request cannot be written --
   *          notably a 2.0 request for a coverage whose axes are unknown
   *          or unrecognisable, which must be refused rather than guessed
   *          at: a subset naming an axis the coverage does not have is
   *          answered with an error status, and a caller that reads only
   *          the status will mistake it for a version it should retry.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString buildGetCoverageUrl(
    const QString &serviceUrl, const QString &version,
    const WcsGetCoverageRequest &request,
    const WcsCoverageDescription &description);

  /*!
   * \brief Builds a DescribeCoverage URL.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString buildDescribeCoverageUrl(
    const QString &serviceUrl, const QString &version,
    const QString &coverageId);

  /*!
   * \brief The next version to try after \a version was refused.
   *
   * \returns The next one down, or empty when there is none left.
   *
   * One ladder, deliberately. The port this came from had two identical
   * copies -- one for capabilities, one for GetCoverage -- whose comment
   * claimed they differed. What differs is which request gets retried,
   * which is the caller's business, not the ladder's.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString nextWcsVersion(
    const QString &version);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_WCSREQUEST_H
