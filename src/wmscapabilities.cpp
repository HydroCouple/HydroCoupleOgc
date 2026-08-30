// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/wmscapabilities.h"

#include <QXmlStreamReader>

namespace HydroCouple::Ogc
{
  namespace
  {
    /*!
     * \brief What a layer takes from the layers it sits inside.
     *
     * Carried down as the tree is walked rather than resolved afterwards
     * from parent pointers: the document states a layer's own metadata
     * before its nested layers, so one pass in document order is enough.
     */
    struct Inherited
    {
        QStringList crs;
        QStringList styles;
        QRectF bounds;
        bool queryable = false;
    };

    //! \returns \a value as a number, or \a fallback when it is not one.
    double toDouble(const QString &value, double fallback)
    {
      bool ok = false;
      const double parsed = value.toDouble(&ok);

      return ok ? parsed : fallback;
    }

    /*!
     * \brief The geographic box 1.3.0 states in child elements.
     *
     * Returned lon/lat whatever the document's axis convention, so nothing
     * downstream has to ask which way round it is.
     */
    QRectF readExGeographicBoundingBox(QXmlStreamReader &reader)
    {
      double west = 0.0;
      double east = 0.0;
      double south = 0.0;
      double north = 0.0;

      while (!reader.atEnd())
      {
        reader.readNext();

        if (reader.isEndElement()
            && reader.name() == QLatin1String("EX_GeographicBoundingBox"))
        {
          break;
        }

        if (!reader.isStartElement())
        {
          continue;
        }

        const QString element = reader.name().toString();
        const QString text = reader.readElementText();

        if (element == QLatin1String("westBoundLongitude"))
        {
          west = toDouble(text, west);
        }
        else if (element == QLatin1String("eastBoundLongitude"))
        {
          east = toDouble(text, east);
        }
        else if (element == QLatin1String("southBoundLatitude"))
        {
          south = toDouble(text, south);
        }
        else if (element == QLatin1String("northBoundLatitude"))
        {
          north = toDouble(text, north);
        }
      }

      return QRectF(QPointF(west, south), QPointF(east, north)).normalized();
    }

    //! The geographic box 1.1.1 states in attributes.
    QRectF readLatLonBoundingBox(const QXmlStreamAttributes &attributes)
    {
      const double west =
        toDouble(attributes.value(QLatin1String("minx")).toString(), 0.0);
      const double east =
        toDouble(attributes.value(QLatin1String("maxx")).toString(), 0.0);
      const double south =
        toDouble(attributes.value(QLatin1String("miny")).toString(), 0.0);
      const double north =
        toDouble(attributes.value(QLatin1String("maxy")).toString(), 0.0);

      return QRectF(QPointF(west, south), QPointF(east, north)).normalized();
    }

    //! Appends what \a into does not already carry, keeping order.
    void mergeInto(QStringList &into, const QString &entry)
    {
      const QString trimmed = entry.trimmed();

      if (!trimmed.isEmpty() && !into.contains(trimmed))
      {
        into.append(trimmed);
      }
    }

    /*!
     * \brief Reads one <Layer> and everything nested inside it.
     *
     * \param reader Positioned on the layer's start element.
     * \param inherited What the enclosing layers established.
     * \param crsElement "CRS" in 1.3.0 and "SRS" in 1.1.1 — the same idea
     *        under two spellings, and the only structural difference
     *        between the versions this parser has to care about.
     * \param[out] layers Where the layer and its descendants are appended.
     */
    void readLayer(QXmlStreamReader &reader, const Inherited &inherited,
                   const QString &crsElement, QList<WmsLayerInfo> &layers)
    {
      WmsLayerInfo layer;

      // Seeded from the enclosing layers rather than started empty: a child
      // ADDS to what its parents declared. Started empty, every layer on a
      // server that states its coordinate systems once at the root comes out
      // drawable in none of them.
      layer.crsIdentifiers = inherited.crs;
      layer.styles = inherited.styles;
      layer.geographicBounds = inherited.bounds;
      layer.queryable = inherited.queryable;

      if (reader.attributes().hasAttribute(QLatin1String("queryable")))
      {
        layer.queryable =
          reader.attributes().value(QLatin1String("queryable"))
          == QLatin1String("1");
      }

      // The slot is claimed before descending, so a parent sits ahead of its
      // children and the list reads in the order a tree would draw. It is
      // rewritten below as the layer's own elements are read.
      const int slot = layers.size();
      layers.append(layer);

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

        if (element == QLatin1String("Layer"))
        {
          // Everything this layer declares has been read by now — a
          // conforming document puts a layer's own metadata before its
          // nested ones — so what the children inherit is complete.
          layers[slot] = layer;

          Inherited forChildren;
          forChildren.crs = layer.crsIdentifiers;
          forChildren.styles = layer.styles;
          forChildren.bounds = layer.geographicBounds;
          forChildren.queryable = layer.queryable;

          readLayer(reader, forChildren, crsElement, layers);

          continue;
        }

        if (element == QLatin1String("Name"))
        {
          layer.name = reader.readElementText();
        }
        else if (element == QLatin1String("Title"))
        {
          layer.title = reader.readElementText();
        }
        else if (element == QLatin1String("Abstract"))
        {
          layer.abstractText = reader.readElementText();
        }
        else if (element == crsElement)
        {
          mergeInto(layer.crsIdentifiers, reader.readElementText());
        }
        else if (element == QLatin1String("Style"))
        {
          // The style's own name only. A Style carries a Title and legend
          // URLs of its own, and a bare search for Name inside it would
          // collect those too.
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
                     && reader.name() == QLatin1String("Name"))
            {
              mergeInto(layer.styles, reader.readElementText());
            }
          }
        }
        else if (element == QLatin1String("EX_GeographicBoundingBox"))
        {
          layer.geographicBounds = readExGeographicBoundingBox(reader);
        }
        else if (element == QLatin1String("LatLonBoundingBox"))
        {
          layer.geographicBounds = readLatLonBoundingBox(reader.attributes());
        }
      }

      layers[slot] = layer;
    }
  }

  QList<WmsLayerInfo> WmsCapabilities::requestableLayers() const
  {
    QList<WmsLayerInfo> requestable;

    for (const WmsLayerInfo &layer : layers)
    {
      if (layer.isRequestable())
      {
        requestable.append(layer);
      }
    }

    return requestable;
  }

  WmsCapabilities parseWmsCapabilities(const QByteArray &xml)
  {
    WmsCapabilities capabilities;

    if (xml.isEmpty())
    {
      capabilities.message =
        QStringLiteral("The server sent an empty response.");

      return capabilities;
    }

    QXmlStreamReader reader(xml);
    QString exception;
    bool sawRoot = false;

    while (!reader.atEnd())
    {
      reader.readNext();

      if (!reader.isStartElement())
      {
        continue;
      }

      const QString element = reader.name().toString();

      // An exception arrives with HTTP 200 and, often, an image content
      // type, so the body is the only place the failure is stated.
      if (element == QLatin1String("ServiceException")
          || element == QLatin1String("ExceptionText"))
      {
        const QString text = reader.readElementText().trimmed();

        if (!text.isEmpty() && exception.isEmpty())
        {
          exception = text;
        }

        continue;
      }

      if (element == QLatin1String("WMS_Capabilities")
          || element == QLatin1String("WMT_MS_Capabilities"))
      {
        sawRoot = true;

        // The version the document declares, not the one that was asked
        // for: a server may answer a 1.3.0 request in 1.1.1, and the CRS
        // spelling and bounding-box axis order follow the answer.
        capabilities.version =
          reader.attributes().value(QLatin1String("version")).toString();

        continue;
      }

      if (element == QLatin1String("Layer"))
      {
        const QString crsElement =
          capabilities.version.startsWith(QLatin1String("1.3"))
            ? QStringLiteral("CRS")
            : QStringLiteral("SRS");

        readLayer(reader, Inherited{}, crsElement, capabilities.layers);

        continue;
      }

      if (element == QLatin1String("Title") && capabilities.title.isEmpty())
      {
        capabilities.title = reader.readElementText();
      }
      else if (element == QLatin1String("Abstract")
               && capabilities.abstractText.isEmpty())
      {
        capabilities.abstractText = reader.readElementText();
      }
      else if (element == QLatin1String("Format"))
      {
        const QString format = reader.readElementText().trimmed();

        if (format.startsWith(QLatin1String("image/"))
            && !capabilities.imageFormats.contains(format))
        {
          capabilities.imageFormats.append(format);
        }
      }
      else if (element == QLatin1String("OnlineResource")
               && capabilities.getMapUrl.isEmpty())
      {
        capabilities.getMapUrl =
          reader.attributes()
            .value(QLatin1String("http://www.w3.org/1999/xlink"),
                   QLatin1String("href"))
            .toString();
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
        QStringLiteral("The response is not a WMS capabilities document.");

      return capabilities;
    }

    capabilities.ok = true;

    return capabilities;
  }

} // namespace HydroCouple::Ogc
