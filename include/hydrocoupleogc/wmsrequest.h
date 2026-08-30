// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   wmsrequest.h
 * \author Caleb Buahin
 * \brief  Asking a WMS for a picture of somewhere.
 *
 * Unlike WMTS, a WMS takes a bounding box, so a client may ask for any
 * rectangle at any size. The awkwardness is entirely in two details that a
 * request either gets right or produces a plausible-looking map from: which
 * parameter the coordinate system is named in, and which way round the two
 * numbers of each corner go.
 */

#ifndef HYDROCOUPLEOGC_WMSREQUEST_H
#define HYDROCOUPLEOGC_WMSREQUEST_H

#include "hydrocoupleogc/export.h"
#include "hydrocoupleogc/wmscapabilities.h"

#include <QRectF>
#include <QString>
#include <QStringList>

namespace HydroCouple::Ogc
{
  /*!
   * \brief One GetMap.
   */
  struct HYDROCOUPLEOGC_EXPORT WmsGetMapRequest
  {
      //! Layer names, drawn bottom to top as listed.
      QStringList layers;

      //! Style per layer, or empty for each layer's default.
      QStringList styles;

      /*!
       * \brief The system \a extent is expressed in, as the server spells
       *        it.
       */
      QString crs;

      //! The ground wanted, in \a crs, always as minimum then maximum.
      QRectF extent;

      int widthPixels = 256;
      int heightPixels = 256;

      //! An advertised format; the first advertised one when empty.
      QString format;

      /*!
       * \brief Whether the image should have a transparent background.
       *
       * On for anything drawn over a basemap, which for this application is
       * everything except the basemap itself.
       */
      bool transparent = true;

      //! The colour behind a non-transparent image, as 0xRRGGBB.
      QString backgroundColour;
  };

  /*!
   * \brief Builds a GetMap URL.
   *
   * Two things follow from the document's declared version rather than from
   * the version that was asked for:
   *
   * - the coordinate system is named `SRS` before 1.3.0 and `CRS` from
   *   1.3.0 on;
   * - from 1.3.0 on the bounding box is written in the **authority's** axis
   *   order, so a request in EPSG:4326 puts latitude first, while one in a
   *   projected system such as EPSG:3857 — or in CRS:84, which exists
   *   precisely to be longitude-first — does not. A client that swaps for
   *   every 1.3.0 request draws the world sideways; one that never swaps
   *   draws a geographic request somewhere else entirely.
   *
   * \param capabilities What the server published.
   * \param request What to ask for.
   * \returns The URL, or empty when the request cannot be built.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString buildGetMapUrl(
    const WmsCapabilities &capabilities, const WmsGetMapRequest &request);

  /*!
   * \brief Whether \a crs is written latitude first under WMS \a version.
   *
   * \param crs The system, in any of the spellings servers use.
   * \param version The version the capabilities document declared.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT bool wmsAxisOrderIsLatitudeFirst(
    const QString &crs, const QString &version);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_WMSREQUEST_H
