// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   wfsrequest.h
 * \author Caleb Buahin
 * \brief  Asking a WFS for features, and for not too many of them.
 */

#ifndef HYDROCOUPLEOGC_WFSREQUEST_H
#define HYDROCOUPLEOGC_WFSREQUEST_H

#include "hydrocoupleogc/export.h"
#include "hydrocoupleogc/wfscapabilities.h"

#include <QRectF>
#include <QString>

namespace HydroCouple::Ogc
{
  /*!
   * \brief One GetFeature.
   */
  struct HYDROCOUPLEOGC_EXPORT WfsGetFeatureRequest
  {
      //! The feature type, as the server names it.
      QString typeName;

      /*!
       * \brief The system to ask for the features in, as the server spells
       *        it.
       *
       * Empty asks for the type's own default, which every server can
       * always answer.
       */
      QString crs;

      /*!
       * \brief The ground wanted, in \a crs, or a null rectangle for all
       *        of it.
       *
       * Worth setting for almost anything: a national feature service holds
       * millions of features and a model needs the ones over one catchment.
       */
      QRectF extent;

      /*!
       * \brief The most features to return.
       *
       * Defaulted rather than left open. A GetFeature with no limit against
       * a national service is a request for every building in the country,
       * and the server will try.
       */
      int count = 5000;

      //! Where in the result set to start, for fetching the next page.
      int startIndex = 0;

      //! An advertised format, or empty to let one be chosen.
      QString outputFormat;
  };

  /*!
   * \brief Builds a GetFeature URL.
   *
   * The spelling follows the version the document DECLARED, which differs
   * more than WMS's does: 2.0.0 asks by TYPENAMES and limits by COUNT,
   * while 1.1.0 and 1.0.0 ask by TYPENAME and limit by MAXFEATURES. A
   * request in the wrong dialect is not refused — a server that does not
   * recognise MAXFEATURES ignores it and returns everything it has.
   *
   * A bounding box is written in the authority's axis order from 1.1.0 on,
   * exactly as a WMS 1.3.0 one is, and 2.0.0 appends the system to the box
   * so the server need not guess which order was meant.
   *
   * \param capabilities What the server published.
   * \param request What to ask for.
   * \returns The URL, or empty when the request cannot be built.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString buildGetFeatureUrl(
    const WfsCapabilities &capabilities, const WfsGetFeatureRequest &request);

  /*!
   * \brief The best format to ask \a type for, of those it offers.
   *
   * GeoJSON when there is any, because it needs no schema to read and
   * carries its own coordinate system; GML otherwise, which every WFS has
   * and which needs a driver. Empty when the type offers neither, which is
   * a service this application cannot read.
   *
   * \param type The feature type.
   * \param serviceFormats What the service offers for types that name none.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString preferredOutputFormat(
    const WfsFeatureType &type, const QStringList &serviceFormats);

  //! \returns Whether \a format is a GeoJSON one.
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT bool isGeoJsonFormat(
    const QString &format);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_WFSREQUEST_H
