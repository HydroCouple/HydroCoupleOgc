// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   wmtscapabilities.h
 * \author Caleb Buahin
 * \brief  What a WMTS server offers, and how to ask it for a tile.
 *
 * WMTS differs from WMS in the thing that matters most to a client: it does
 * not take a bounding box. It publishes a fixed pyramid of tile matrices and
 * serves the tiles of it, so a client has to read that pyramid before it can
 * ask for anything at all.
 */

#ifndef HYDROCOUPLEOGC_WMTSCAPABILITIES_H
#define HYDROCOUPLEOGC_WMTSCAPABILITIES_H

#include "hydrocoupleogc/crsurn.h"
#include "hydrocoupleogc/export.h"

#include <QByteArray>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace HydroCouple::Ogc
{
  /*!
   * \brief One level of a tile pyramid.
   */
  struct HYDROCOUPLEOGC_EXPORT WmtsTileMatrix
  {
      //! What a GetTile calls this level; often but not always the zoom.
      QString identifier;

      /*!
       * \brief The map scale this level draws at.
       *
       * How a level is chosen: the standard fixes a pixel at 0.28 mm, so a
       * scale denominator is metres per pixel regardless of the system the
       * matrix set is in.
       */
      double scaleDenominator = 0.0;

      //! The corner tile (0, 0) hangs from, in the matrix set's own CRS.
      QPointF topLeft;

      int tileWidth = 256;
      int tileHeight = 256;

      //! How many tiles across and down this level has.
      int matrixWidth = 0;
      int matrixHeight = 0;

      //! Metres of ground per pixel at this level.
      [[nodiscard]] double metresPerPixel() const;
  };

  /*!
   * \brief A pyramid, and the system it is laid out in.
   */
  struct HYDROCOUPLEOGC_EXPORT WmtsTileMatrixSet
  {
      QString identifier;

      //! As the document spells it, kept for the request that echoes it.
      QString supportedCrs;

      //! The same thing resolved; see crsurn.h for why that is not trivial.
      CrsIdentifier crs;

      //! Coarsest first is usual but not required; nothing here assumes it.
      QList<WmtsTileMatrix> matrices;

      /*!
       * \brief Whether this is the familiar web-map pyramid.
       *
       * Web Mercator, 256-pixel tiles, one tile at the top hanging from the
       * corner of the world. When it is, a client that already draws XYZ
       * tiles can draw this without knowing anything about matrix sets;
       * when it is not, the tiles are laid out on a grid of the server's
       * choosing and only a matrix-aware client can place them.
       */
      [[nodiscard]] bool isWebMercatorQuad() const;

      /*!
       * \brief The level to draw a map of \a metresPerPixel from.
       *
       * The coarsest level whose pixels still cover no more ground than the
       * map's: drawing from anything coarser is a blurred map, and drawing
       * from anything finer fetches more tiles than the screen can show.
       * When every level is coarser -- the map is zoomed in past the bottom
       * of the pyramid -- the finest level there is comes closest.
       *
       * Searched rather than indexed, because the levels arrive in whatever
       * order the document lists them and their identifiers are not always
       * numbers.
       *
       * \returns The level, or nullptr when the set has none.
       */
      [[nodiscard]] const WmtsTileMatrix *matrixFor(double metresPerPixel)
        const;
  };

  /*!
   * \brief A URL template the server publishes for fetching tiles directly.
   */
  struct HYDROCOUPLEOGC_EXPORT WmtsResourceTemplate
  {
      QString format;
      QString resourceType;

      //! With {Style}, {TileMatrixSet}, {TileMatrix}, {TileRow}, {TileCol}.
      QString templateUrl;
  };

  /*!
   * \brief One layer a WMTS offers.
   */
  struct HYDROCOUPLEOGC_EXPORT WmtsLayerInfo
  {
      QString identifier;
      QString title;
      QString abstractText;

      QStringList formats;
      QStringList styles;

      //! The style used when a request does not name one.
      QString defaultStyle;

      //! The pyramids this layer is published on, by identifier.
      QStringList tileMatrixSetIds;

      //! Longitude/latitude, from ows:WGS84BoundingBox.
      QRectF geographicBounds;

      //! Templates for RESTful addressing; empty when only KVP is offered.
      QList<WmtsResourceTemplate> resourceTemplates;
  };

  /*!
   * \brief What a WMTS server said about itself.
   */
  struct HYDROCOUPLEOGC_EXPORT WmtsCapabilities
  {
      bool ok = false;
      QString message;
      QString version;
      QString title;

      //! Where a KVP GetTile goes; empty when the server offers only REST.
      QString getTileKvpUrl;

      QList<WmtsLayerInfo> layers;
      QList<WmtsTileMatrixSet> matrixSets;

      //! \returns The set with \a identifier, or nullptr.
      [[nodiscard]] const WmtsTileMatrixSet *matrixSet(
        const QString &identifier) const;
  };

  /*!
   * \brief Reads a WMTS GetCapabilities response.
   * \param xml The response body.
   * \returns What the server offers; check \a ok first.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT WmtsCapabilities
  parseWmtsCapabilities(const QByteArray &xml);

  /*!
   * \brief Builds the URL for one tile.
   *
   * **A published template is used when there is one**, and a KVP query
   * built only when there is not. Servers that offer both often serve the
   * templates from a pool of hosts and the KVP endpoint from one, so
   * preferring the template is not merely a matter of taste — and a client
   * that only ever builds KVP cannot talk to a REST-only server at all.
   *
   * \param capabilities What the server published.
   * \param layer The layer to fetch from.
   * \param matrixSetId Which pyramid.
   * \param matrixId Which level of it.
   * \param row Tile row.
   * \param column Tile column.
   * \param style The style, or empty for the layer's default.
   * \param format The image format, or empty for the layer's first.
   * \returns The URL, or an empty string when the request cannot be built.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString buildWmtsTileUrl(
    const WmtsCapabilities &capabilities, const WmtsLayerInfo &layer,
    const QString &matrixSetId, const QString &matrixId, int row, int column,
    const QString &style = QString(), const QString &format = QString());

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_WMTSCAPABILITIES_H
