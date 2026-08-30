// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   wmscapabilities.h
 * \author Caleb Buahin
 * \brief  What a WMS server says it can do, read off its GetCapabilities.
 *
 * A pure function over bytes. It performs no I/O, owns no network, and knows
 * nothing about layers or painting, so the awkward half of talking to a WMS
 * — which is reading what the server claims — can be checked against saved
 * responses from real servers rather than against a live one.
 *
 * That is the whole reason this is separate from the layer that draws the
 * imagery. The same code in openswmm.gui is entangled with QNetworkReply
 * slots and layer state, and has no tests at all.
 */

#ifndef HYDROCOUPLEOGC_WMSCAPABILITIES_H
#define HYDROCOUPLEOGC_WMSCAPABILITIES_H

#include "hydrocoupleogc/export.h"

#include <QByteArray>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace HydroCouple::Ogc
{
  /*!
   * \brief One layer a WMS offers.
   *
   * Flattened out of the capabilities tree, but *after* inheritance has been
   * applied — see parseWmsCapabilities() for why that distinction is the
   * whole job.
   */
  struct HYDROCOUPLEOGC_EXPORT WmsLayerInfo
  {
      //! The identifier a GetMap asks for; empty for a group layer.
      QString name;

      QString title;
      QString abstractText;

      /*!
       * \brief The coordinate systems this layer can be drawn in.
       *
       * Includes everything inherited from the layers it sits inside, which
       * is usually where they are actually declared.
       */
      QStringList crsIdentifiers;

      //! Style names, inherited ones included.
      QStringList styles;

      /*!
       * \brief The ground the layer covers, in longitude/latitude degrees.
       *
       * From EX_GeographicBoundingBox (1.3.0) or LatLonBoundingBox (1.1.1),
       * inherited when the layer does not give its own. Always lon/lat here
       * whatever the document's axis order, so a caller never has to ask.
       */
      QRectF geographicBounds;

      //! Whether GetFeatureInfo may be asked about this layer.
      bool queryable = false;

      /*!
       * \brief Whether a GetMap can name this layer.
       *
       * A layer with no name is a heading in the server's own tree — a
       * grouping, not something that can be drawn. They are kept rather than
       * dropped, because a tree with its headings removed is a different
       * shape from the one the server published.
       */
      [[nodiscard]] bool isRequestable() const { return !name.isEmpty(); }
  };

  /*!
   * \brief What a WMS server said about itself.
   */
  struct HYDROCOUPLEOGC_EXPORT WmsCapabilities
  {
      //! Whether the document parsed at all.
      bool ok = false;

      //! What went wrong, or what the server's exception report said.
      QString message;

      /*!
       * \brief The version the *document* declares.
       *
       * Not the one that was asked for: a server may answer a 1.3.0 request
       * with 1.1.1, and everything downstream — the CRS parameter's spelling
       * and the axis order of a geographic bounding box — follows the answer
       * rather than the question.
       */
      QString version;

      QString title;
      QString abstractText;

      //! Where GetMap requests go, which need not be where capabilities came
      //! from.
      QString getMapUrl;

      //! Image formats GetMap advertises.
      QStringList imageFormats;

      //! Every layer, parents before children, inheritance already applied.
      QList<WmsLayerInfo> layers;

      //! \returns Only the layers a GetMap can name.
      [[nodiscard]] QList<WmsLayerInfo> requestableLayers() const;
  };

  /*!
   * \brief Reads a WMS GetCapabilities response.
   *
   * **Nested layers inherit from their parents**, which is the one thing a
   * casual reading of the document gets wrong. WMS declares a tree, and the
   * standard (OGC 06-042 §7.2.4.8) says a child layer *adds to* its
   * parent's coordinate systems, styles and bounding box rather than
   * replacing them. Servers rely on it heavily: on a common OSM WMS the root
   * carries twenty-four coordinate systems and every drawable layer beneath
   * it declares none of its own. Read flat, every one of those layers looks
   * as though it can be drawn in no coordinate system at all.
   *
   * The response may also be an exception report, and servers send those
   * with HTTP 200 and an image content type, so the only way to know is to
   * look. \a ok is then false and \a message carries what the server said.
   *
   * \param xml The response body.
   * \returns What the server offers; check \a ok first.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT WmsCapabilities
  parseWmsCapabilities(const QByteArray &xml);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_WMSCAPABILITIES_H
