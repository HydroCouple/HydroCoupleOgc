// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/wfscapabilities.h"

#include <QPointF>
#include <QXmlStreamReader>

namespace HydroCouple::Ogc
{
  namespace
  {
    //! Reads an "x y" pair as the document writes it.
    QPointF readPair(const QString &text)
    {
      const QStringList parts =
        text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);

      if (parts.size() < 2)
      {
        return {};
      }

      return QPointF(parts.at(0).toDouble(), parts.at(1).toDouble());
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

        // ows:WGS84BoundingBox is CRS84 by definition: longitude then
        // latitude, whatever an EPSG:4326 box in the same document does.
        if (reader.name() == QLatin1String("LowerCorner"))
        {
          lower = readPair(reader.readElementText());
        }
        else if (reader.name() == QLatin1String("UpperCorner"))
        {
          upper = readPair(reader.readElementText());
        }
      }

      return QRectF(lower, upper).normalized();
    }

    WfsFeatureType readFeatureType(QXmlStreamReader &reader)
    {
      WfsFeatureType type;
      int depth = 1;

      while (!reader.atEnd() && depth > 0)
      {
        reader.readNext();

        if (reader.isEndElement())
        {
          if (reader.name() == QLatin1String("FeatureType"))
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

        if (element == QLatin1String("Name") && type.name.isEmpty())
        {
          type.name = reader.readElementText().trimmed();
        }
        else if (element == QLatin1String("Title") && type.title.isEmpty())
        {
          type.title = reader.readElementText().trimmed();
        }
        else if (element == QLatin1String("Abstract")
                 && type.abstractText.isEmpty())
        {
          type.abstractText = reader.readElementText().trimmed();
        }
        else if (element == QLatin1String("DefaultCRS")
                 || element == QLatin1String("DefaultSRS"))
        {
          type.defaultCrs = reader.readElementText().trimmed();
        }
        else if (element == QLatin1String("OtherCRS")
                 || element == QLatin1String("OtherSRS"))
        {
          const QString crs = reader.readElementText().trimmed();

          if (!crs.isEmpty())
          {
            type.otherCrs.append(crs);
          }
        }
        else if (element == QLatin1String("Format"))
        {
          const QString format = reader.readElementText().trimmed();

          if (!format.isEmpty() && !type.outputFormats.contains(format))
          {
            type.outputFormats.append(format);
          }
        }
        else if (element == QLatin1String("WGS84BoundingBox"))
        {
          type.geographicBounds = readWgs84BoundingBox(reader);
        }
      }

      return type;
    }
  }

  QStringList WfsFeatureType::allCrs() const
  {
    QStringList all;

    if (!defaultCrs.isEmpty())
    {
      all.append(defaultCrs);
    }

    for (const QString &crs : otherCrs)
    {
      if (!all.contains(crs))
      {
        all.append(crs);
      }
    }

    return all;
  }

  QString WfsFeatureType::spellingOf(const QString &wanted) const
  {
    const CrsIdentifier target = parseCrsIdentifier(wanted);

    if (!target.isValid())
    {
      return {};
    }

    // Compared as authority and code rather than as text, because a server
    // that publishes urn:ogc:def:crs:EPSG::4326 has published EPSG:4326 --
    // and the request must go back in the server's own spelling.
    for (const QString &advertised : allCrs())
    {
      if (parseCrsIdentifier(advertised) == target)
      {
        return advertised;
      }
    }

    return {};
  }

  const WfsFeatureType *WfsCapabilities::featureType(const QString &name) const
  {
    for (const WfsFeatureType &type : featureTypes)
    {
      if (type.name == name)
      {
        return &type;
      }
    }

    return nullptr;
  }

  WfsCapabilities parseWfsCapabilities(const QByteArray &xml)
  {
    WfsCapabilities capabilities;

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
    QString currentParameter;

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

      if (element == QLatin1String("WFS_Capabilities"))
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
        currentParameter.clear();

        continue;
      }

      if (element == QLatin1String("Parameter"))
      {
        currentParameter =
          reader.attributes().value(QLatin1String("name")).toString();

        continue;
      }

      if (element == QLatin1String("Get")
          && currentOperation == QLatin1String("GetFeature")
          && capabilities.getFeatureUrl.isEmpty())
      {
        capabilities.getFeatureUrl =
          reader.attributes()
            .value(QLatin1String("http://www.w3.org/1999/xlink"),
                   QLatin1String("href"))
            .toString();

        continue;
      }

      // The service-level formats are the allowed values of GetFeature's
      // outputFormat parameter. Read only under that parameter: the same
      // element name carries the allowed values of every other one.
      if (element == QLatin1String("Value")
          && currentOperation == QLatin1String("GetFeature")
          && currentParameter == QLatin1String("outputFormat"))
      {
        const QString format = reader.readElementText().trimmed();

        if (!format.isEmpty() && !capabilities.outputFormats.contains(format))
        {
          capabilities.outputFormats.append(format);
        }

        continue;
      }

      if (element == QLatin1String("FeatureType"))
      {
        capabilities.featureTypes.append(readFeatureType(reader));

        continue;
      }

      if (element == QLatin1String("Title") && capabilities.title.isEmpty())
      {
        capabilities.title = reader.readElementText().trimmed();

        continue;
      }

      if (element == QLatin1String("Abstract")
          && capabilities.abstractText.isEmpty())
      {
        capabilities.abstractText = reader.readElementText().trimmed();
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
        QStringLiteral("The response is not a WFS capabilities document.");

      return capabilities;
    }

    capabilities.ok = true;

    return capabilities;
  }

} // namespace HydroCouple::Ogc
