// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   wfscapabilities.h
 * \author Caleb Buahin
 * \brief  What a WFS server offers, and how to ask it for features.
 *
 * WMS and WMTS answer with pictures of data. A WFS answers with the data,
 * which is what makes it the one service in this library that can put a
 * catchment boundary into a model rather than merely behind one.
 *
 * Its capabilities document is shaped like WMTS's — OWS operations
 * metadata, a flat list of what is published — with one difference that
 * matters: what a feature type is published in and what it can be asked
 * for are per type, not per service. A server may hold one layer in the
 * national grid and another in WGS 84.
 */

#ifndef HYDROCOUPLEOGC_WFSCAPABILITIES_H
#define HYDROCOUPLEOGC_WFSCAPABILITIES_H

#include "hydrocoupleogc/crsurn.h"
#include "hydrocoupleogc/export.h"

#include <QByteArray>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace HydroCouple::Ogc
{
  /*!
   * \brief One collection of features a WFS publishes.
   */
  struct HYDROCOUPLEOGC_EXPORT WfsFeatureType
  {
      //! What a GetFeature asks for, usually prefixed: "bag:pand".
      QString name;

      QString title;
      QString abstractText;

      /*!
       * \brief The system the server returns this type in unless told
       *        otherwise.
       *
       * Kept as the server spells it, because that is what a request has to
       * echo back.
       */
      QString defaultCrs;

      //! Everything else it can be asked for in, as the server spells them.
      QStringList otherCrs;

      /*!
       * \brief Formats this type can be returned in.
       *
       * Per type rather than per service, and the reason to read them: a
       * server that offers GeoJSON for one collection and GML alone for
       * another is common, and the two need entirely different decoding.
       */
      QStringList outputFormats;

      //! Longitude/latitude degrees, from ows:WGS84BoundingBox.
      QRectF geographicBounds;

      //! Every system this type can be asked for in, default first.
      [[nodiscard]] QStringList allCrs() const;

      /*!
       * \brief How this type is best asked for in \a wanted.
       *
       * \param wanted An authority and code, e.g. "EPSG:4326".
       * \returns The server's own spelling of it, or empty when the server
       *          does not publish this type in that system.
       */
      [[nodiscard]] QString spellingOf(const QString &wanted) const;
  };

  /*!
   * \brief What a WFS server said about itself.
   */
  struct HYDROCOUPLEOGC_EXPORT WfsCapabilities
  {
      bool ok = false;
      QString message;

      /*!
       * \brief The version the document declares.
       *
       * Which half the request's spelling follows: 2.0.0 asks for
       * TYPENAMES and COUNT where 1.1.0 asks for TYPENAME and MAXFEATURES.
       */
      QString version;

      QString title;
      QString abstractText;

      //! Where GetFeature requests go, which need not be where this came
      //! from.
      QString getFeatureUrl;

      //! Formats the service advertises, for types that name none of their
      //! own.
      QStringList outputFormats;

      QList<WfsFeatureType> featureTypes;

      //! \returns The type with \a name, or nullptr.
      [[nodiscard]] const WfsFeatureType *featureType(const QString &name)
        const;
  };

  /*!
   * \brief Reads a WFS GetCapabilities response.
   * \param xml The response body.
   * \returns What the server offers; check \a ok first.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT WfsCapabilities
  parseWfsCapabilities(const QByteArray &xml);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_WFSCAPABILITIES_H
