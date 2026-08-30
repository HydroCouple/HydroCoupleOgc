// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/wmtscapabilities.h"

#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

#include <cmath>

namespace HydroCouple::Ogc
{
  namespace
  {
    /*!
     * \brief The size the standard fixes a pixel at, in metres.
     *
     * WMTS defines a scale denominator against a 0.28 mm pixel, which is
     * what makes a denominator convertible to ground resolution without
     * knowing anything about the display.
     */
    constexpr double kStandardPixelMetres = 0.00028;

    //! \returns \a value as a number, or \a fallback.
    double toDouble(const QString &value, double fallback)
    {
      bool ok = false;
      const double parsed = value.toDouble(&ok);

      return ok ? parsed : fallback;
    }

    //! \returns \a value as an integer, or \a fallback.
    int toInt(const QString &value, int fallback)
    {
      bool ok = false;
      const int parsed = value.toInt(&ok);

      return ok ? parsed : fallback;
    }

    /*!
     * \brief Reads an "x y" pair.
     *
     * The order is the axis order of the matrix set's own CRS, which for a
     * projected one is easting then northing. Nothing is swapped here: a
     * corner is stored as the document states it and the caller, which
     * knows the CRS, decides what the two numbers mean.
     */
    QPointF readPair(const QString &text)
    {
      const QStringList parts =
        text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);

      if (parts.size() < 2)
      {
        return {};
      }

      return QPointF(toDouble(parts.at(0), 0.0), toDouble(parts.at(1), 0.0));
    }

    WmtsTileMatrix readTileMatrix(QXmlStreamReader &reader)
    {
      WmtsTileMatrix matrix;
      int depth = 1;

      while (!reader.atEnd() && depth > 0)
      {
        reader.readNext();

        if (reader.isEndElement())
        {
          if (reader.name() == QLatin1String("TileMatrix"))
          {
            --depth;
          }

          continue;
        }

        if (!reader.isStartElement())
        {
          continue;
        }

        const QString element = reader.name().toString();

        if (element == QLatin1String("Identifier"))
        {
          matrix.identifier = reader.readElementText().trimmed();
        }
        else if (element == QLatin1String("ScaleDenominator"))
        {
          matrix.scaleDenominator = toDouble(reader.readElementText(), 0.0);
        }
        else if (element == QLatin1String("TopLeftCorner"))
        {
          matrix.topLeft = readPair(reader.readElementText());
        }
        else if (element == QLatin1String("TileWidth"))
        {
          matrix.tileWidth = toInt(reader.readElementText(), 256);
        }
        else if (element == QLatin1String("TileHeight"))
        {
          matrix.tileHeight = toInt(reader.readElementText(), 256);
        }
        else if (element == QLatin1String("MatrixWidth"))
        {
          matrix.matrixWidth = toInt(reader.readElementText(), 0);
        }
        else if (element == QLatin1String("MatrixHeight"))
        {
          matrix.matrixHeight = toInt(reader.readElementText(), 0);
        }
      }

      return matrix;
    }

    WmtsTileMatrixSet readTileMatrixSet(QXmlStreamReader &reader)
    {
      WmtsTileMatrixSet set;
      int depth = 1;

      while (!reader.atEnd() && depth > 0)
      {
        reader.readNext();

        if (reader.isEndElement())
        {
          if (reader.name() == QLatin1String("TileMatrixSet"))
          {
            --depth;
          }

          continue;
        }

        if (!reader.isStartElement())
        {
          continue;
        }

        const QString element = reader.name().toString();

        if (element == QLatin1String("Identifier") && set.identifier.isEmpty())
        {
          set.identifier = reader.readElementText().trimmed();
        }
        else if (element == QLatin1String("SupportedCRS"))
        {
          set.supportedCrs = reader.readElementText().trimmed();
          set.crs = parseCrsIdentifier(set.supportedCrs);
        }
        else if (element == QLatin1String("TileMatrix"))
        {
          set.matrices.append(readTileMatrix(reader));
        }
      }

      return set;
    }

    QRectF readWgs84BoundingBox(QXmlStreamReader &reader)
    {
      QPointF lower;
      QPointF upper;
      int depth = 1;

      while (!reader.atEnd() && depth > 0)
      {
        reader.readNext();

        if (reader.isEndElement())
        {
          if (reader.name() == QLatin1String("WGS84BoundingBox"))
          {
            --depth;
          }

          continue;
        }

        if (!reader.isStartElement())
        {
          continue;
        }

        const QString element = reader.name().toString();

        // ows:WGS84BoundingBox is CRS84 by definition, so these are
        // longitude then latitude whatever an EPSG:4326 box would be.
        if (element == QLatin1String("LowerCorner"))
        {
          lower = readPair(reader.readElementText());
        }
        else if (element == QLatin1String("UpperCorner"))
        {
          upper = readPair(reader.readElementText());
        }
      }

      return QRectF(lower, upper).normalized();
    }

    WmtsLayerInfo readLayer(QXmlStreamReader &reader)
    {
      WmtsLayerInfo layer;
      int depth = 1;

      while (!reader.atEnd() && depth > 0)
      {
        reader.readNext();

        if (reader.isEndElement())
        {
          if (reader.name() == QLatin1String("Layer"))
          {
            --depth;
          }

          continue;
        }

        if (!reader.isStartElement())
        {
          continue;
        }

        const QString element = reader.name().toString();

        if (element == QLatin1String("Identifier") && layer.identifier.isEmpty())
        {
          layer.identifier = reader.readElementText().trimmed();
        }
        else if (element == QLatin1String("Title") && layer.title.isEmpty())
        {
          layer.title = reader.readElementText();
        }
        else if (element == QLatin1String("Abstract")
                 && layer.abstractText.isEmpty())
        {
          layer.abstractText = reader.readElementText();
        }
        else if (element == QLatin1String("Format"))
        {
          const QString format = reader.readElementText().trimmed();

          if (!format.isEmpty() && !layer.formats.contains(format))
          {
            layer.formats.append(format);
          }
        }
        else if (element == QLatin1String("WGS84BoundingBox"))
        {
          layer.geographicBounds = readWgs84BoundingBox(reader);
        }
        else if (element == QLatin1String("Style"))
        {
          const bool isDefault =
            reader.attributes().value(QLatin1String("isDefault"))
            == QLatin1String("true");

          // A Style holds a Title and legend URLs beside its Identifier, so
          // the first Identifier inside it is taken and the rest of the
          // element skipped.
          QString identifier;
          int styleDepth = 1;

          while (!reader.atEnd() && styleDepth > 0)
          {
            reader.readNext();

            if (reader.isEndElement()
                && reader.name() == QLatin1String("Style"))
            {
              --styleDepth;
            }
            else if (reader.isStartElement()
                     && reader.name() == QLatin1String("Identifier")
                     && identifier.isEmpty())
            {
              identifier = reader.readElementText().trimmed();
            }
          }

          if (!identifier.isEmpty())
          {
            if (!layer.styles.contains(identifier))
            {
              layer.styles.append(identifier);
            }

            if (isDefault || layer.defaultStyle.isEmpty())
            {
              layer.defaultStyle = identifier;
            }
          }
        }
        else if (element == QLatin1String("TileMatrixSetLink"))
        {
          int linkDepth = 1;

          while (!reader.atEnd() && linkDepth > 0)
          {
            reader.readNext();

            if (reader.isEndElement()
                && reader.name() == QLatin1String("TileMatrixSetLink"))
            {
              --linkDepth;
            }
            else if (reader.isStartElement()
                     && reader.name() == QLatin1String("TileMatrixSet"))
            {
              const QString id = reader.readElementText().trimmed();

              if (!id.isEmpty() && !layer.tileMatrixSetIds.contains(id))
              {
                layer.tileMatrixSetIds.append(id);
              }
            }
          }
        }
        else if (element == QLatin1String("ResourceURL"))
        {
          WmtsResourceTemplate resource;
          resource.format =
            reader.attributes().value(QLatin1String("format")).toString();
          resource.resourceType =
            reader.attributes().value(QLatin1String("resourceType")).toString();
          resource.templateUrl =
            reader.attributes().value(QLatin1String("template")).toString();

          if (!resource.templateUrl.isEmpty())
          {
            layer.resourceTemplates.append(resource);
          }
        }
      }

      return layer;
    }
  }

  double WmtsTileMatrix::metresPerPixel() const
  {
    return scaleDenominator * kStandardPixelMetres;
  }

  bool WmtsTileMatrixSet::isWebMercatorQuad() const
  {
    if (crs.authority.compare(QLatin1String("EPSG"), Qt::CaseInsensitive) != 0
        || crs.code != QLatin1String("3857"))
    {
      return false;
    }

    if (matrices.isEmpty())
    {
      return false;
    }

    // Web Mercator alone is not enough: a server may publish a pyramid in
    // 3857 with 512-pixel tiles or an origin of its own, and a client that
    // assumed the familiar grid would ask for tiles that are not there.
    for (const WmtsTileMatrix &matrix : matrices)
    {
      if (matrix.tileWidth != 256 || matrix.tileHeight != 256)
      {
        return false;
      }

      if (std::abs(matrix.topLeft.x() + 20037508.34) > 1.0
          || std::abs(matrix.topLeft.y() - 20037508.34) > 1.0)
      {
        return false;
      }
    }

    return true;
  }

  const WmtsTileMatrix *WmtsTileMatrixSet::matrixFor(double metresPerPixel)
    const
  {
    const WmtsTileMatrix *best = nullptr;

    for (const WmtsTileMatrix &matrix : matrices)
    {
      // The coarsest level whose pixels still cover no more ground than the
      // map's, so a map is never drawn blurred from tiles coarser than its
      // own scale, and no more tiles are fetched than that requires. Chosen
      // by comparing every level rather than by taking the first that fits,
      // because the document may list them in any order.
      if (matrix.metresPerPixel() <= metresPerPixel
          && (!best || matrix.metresPerPixel() > best->metresPerPixel()))
      {
        best = &matrix;
      }
    }

    if (!best)
    {
      // Everything is coarser than asked for -- the map is zoomed in past
      // the bottom of the pyramid -- so the finest level there is comes
      // closest, and the client stretches it.
      for (const WmtsTileMatrix &matrix : matrices)
      {
        if (!best || matrix.metresPerPixel() < best->metresPerPixel())
        {
          best = &matrix;
        }
      }
    }

    return best;
  }

  const WmtsTileMatrixSet *WmtsCapabilities::matrixSet(
    const QString &identifier) const
  {
    for (const WmtsTileMatrixSet &set : matrixSets)
    {
      if (set.identifier == identifier)
      {
        return &set;
      }
    }

    return nullptr;
  }

  WmtsCapabilities parseWmtsCapabilities(const QByteArray &xml)
  {
    WmtsCapabilities capabilities;

    if (xml.isEmpty())
    {
      capabilities.message =
        QStringLiteral("The server sent an empty response.");

      return capabilities;
    }

    QXmlStreamReader reader(xml);
    QString exception;
    bool sawRoot = false;
    QString currentOperation;

    while (!reader.atEnd())
    {
      reader.readNext();

      if (!reader.isStartElement())
      {
        continue;
      }

      const QString element = reader.name().toString();

      if (element == QLatin1String("ExceptionText")
          || element == QLatin1String("ServiceException"))
      {
        const QString text = reader.readElementText().trimmed();

        if (!text.isEmpty() && exception.isEmpty())
        {
          exception = text;
        }

        continue;
      }

      if (element == QLatin1String("Capabilities"))
      {
        sawRoot = true;
        capabilities.version =
          reader.attributes().value(QLatin1String("version")).toString();

        continue;
      }

      if (element == QLatin1String("Operation"))
      {
        currentOperation =
          reader.attributes().value(QLatin1String("name")).toString();

        continue;
      }

      if (element == QLatin1String("Get")
          && currentOperation == QLatin1String("GetTile")
          && capabilities.getTileKvpUrl.isEmpty())
      {
        capabilities.getTileKvpUrl =
          reader.attributes()
            .value(QLatin1String("http://www.w3.org/1999/xlink"),
                   QLatin1String("href"))
            .toString();

        continue;
      }

      if (element == QLatin1String("Layer"))
      {
        capabilities.layers.append(readLayer(reader));

        continue;
      }

      if (element == QLatin1String("TileMatrixSet"))
      {
        // The same element name is used for the reference inside a
        // TileMatrixSetLink, which readLayer() consumes; anything reaching
        // here is a definition.
        capabilities.matrixSets.append(readTileMatrixSet(reader));

        continue;
      }

      if (element == QLatin1String("Title") && capabilities.title.isEmpty())
      {
        capabilities.title = reader.readElementText();
      }
    }

    if (!exception.isEmpty())
    {
      capabilities.message = exception;

      return capabilities;
    }

    if (reader.hasError())
    {
      capabilities.message = reader.errorString();

      return capabilities;
    }

    if (!sawRoot)
    {
      capabilities.message =
        QStringLiteral("The response is not a WMTS capabilities document.");

      return capabilities;
    }

    capabilities.ok = true;

    return capabilities;
  }

  QString buildWmtsTileUrl(const WmtsCapabilities &capabilities,
                           const WmtsLayerInfo &layer,
                           const QString &matrixSetId, const QString &matrixId,
                           int row, int column, const QString &style,
                           const QString &format)
  {
    const QString chosenStyle =
      style.isEmpty() ? layer.defaultStyle : style;
    const QString chosenFormat =
      format.isEmpty() ? (layer.formats.isEmpty() ? QString()
                                                  : layer.formats.first())
                       : format;

    // A published template wins. A server that offers only these cannot be
    // reached by a KVP request at all, and one that offers both usually
    // serves the templates from a pool of hosts and the query endpoint from
    // a single one.
    for (const WmtsResourceTemplate &resource : layer.resourceTemplates)
    {
      if (!resource.resourceType.isEmpty()
          && resource.resourceType != QLatin1String("tile"))
      {
        continue;
      }

      if (!chosenFormat.isEmpty() && !resource.format.isEmpty()
          && resource.format != chosenFormat)
      {
        continue;
      }

      QString url = resource.templateUrl;
      url.replace(QLatin1String("{Style}"), chosenStyle);
      url.replace(QLatin1String("{TileMatrixSet}"), matrixSetId);
      url.replace(QLatin1String("{TileMatrix}"), matrixId);
      url.replace(QLatin1String("{TileRow}"), QString::number(row));
      url.replace(QLatin1String("{TileCol}"), QString::number(column));

      return url;
    }

    if (capabilities.getTileKvpUrl.isEmpty())
    {
      return {};
    }

    QUrl url(capabilities.getTileKvpUrl);
    QUrlQuery query(url.query());

    query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WMTS"));
    query.addQueryItem(QStringLiteral("REQUEST"), QStringLiteral("GetTile"));
    query.addQueryItem(QStringLiteral("VERSION"),
                       capabilities.version.isEmpty()
                         ? QStringLiteral("1.0.0")
                         : capabilities.version);
    query.addQueryItem(QStringLiteral("LAYER"), layer.identifier);

    if (!chosenStyle.isEmpty())
    {
      query.addQueryItem(QStringLiteral("STYLE"), chosenStyle);
    }

    if (!chosenFormat.isEmpty())
    {
      query.addQueryItem(QStringLiteral("FORMAT"), chosenFormat);
    }

    query.addQueryItem(QStringLiteral("TILEMATRIXSET"), matrixSetId);
    query.addQueryItem(QStringLiteral("TILEMATRIX"), matrixId);
    query.addQueryItem(QStringLiteral("TILEROW"), QString::number(row));
    query.addQueryItem(QStringLiteral("TILECOL"), QString::number(column));

    url.setQuery(query);

    return url.toString();
  }

} // namespace HydroCouple::Ogc
