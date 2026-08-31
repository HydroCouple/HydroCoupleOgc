// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/wcscapabilities.h"

#include <QPointF>
#include <QXmlStreamReader>

namespace HydroCouple::Ogc
{
  namespace
  {
    //! Reads a whitespace-separated list of numbers.
    QList<double> readNumbers(const QString &text)
    {
      QList<double> values;

      const QStringList parts =
        text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);

      for (const QString &part : parts)
      {
        bool ok = false;
        const double value = part.toDouble(&ok);

        if (ok)
        {
          values.append(value);
        }
      }

      return values;
    }

    /*!
     * \brief Reads a 1.0.0 <lonLatEnvelope>, whose corners are two
     *        <gml:pos> siblings rather than a named lower and upper.
     *
     * The prefix is the trap. QXmlStreamReader::name() is the local name,
     * so an element written <gml:pos> answers to "pos" and never to
     * "gml:pos" -- a comparison against the prefixed spelling silently
     * matches nothing, and every 1.0.0 coverage ends up with no extent at
     * all. The order of the two is the whole meaning: first is the lower
     * corner, second the upper.
     */
    QRectF readLonLatEnvelope(QXmlStreamReader &reader)
    {
      QList<QPointF> corners;
      int depth = 1;

      while (!reader.atEnd() && depth > 0)
      {
        reader.readNext();

        if (reader.isEndElement())
        {
          if (reader.name() == QLatin1String("lonLatEnvelope"))
          {
            --depth;
          }

          continue;
        }

        if (reader.isStartElement() && reader.name() == QLatin1String("pos"))
        {
          const QList<double> values = readNumbers(reader.readElementText());

          if (values.size() >= 2)
          {
            corners.append(QPointF(values.at(0), values.at(1)));
          }
        }
      }

      if (corners.size() < 2)
      {
        return {};
      }

      return QRectF(corners.at(0), corners.at(1)).normalized();
    }

    //! Reads an ows:WGS84BoundingBox, which is longitude-first by definition.
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

        const QList<double> values = readNumbers(reader.readElementText());

        if (values.size() < 2)
        {
          continue;
        }

        if (reader.name() == QLatin1String("LowerCorner"))
        {
          lower = QPointF(values.at(0), values.at(1));
        }
        else if (reader.name() == QLatin1String("UpperCorner"))
        {
          upper = QPointF(values.at(0), values.at(1));
        }
      }

      return QRectF(lower, upper).normalized();
    }

    /*!
     * \brief Reads one coverage listing, in whichever protocol wrote it.
     *
     * \param reader Positioned on the listing's start element.
     * \param endTag The element this listing ends at.
     */
    WcsCoverageSummary readCoverageSummary(QXmlStreamReader &reader,
                                           const QString &endTag)
    {
      WcsCoverageSummary summary;
      int depth = 1;

      while (!reader.atEnd() && depth > 0)
      {
        reader.readNext();

        if (reader.isEndElement())
        {
          if (reader.name() == endTag)
          {
            --depth;
          }

          continue;
        }

        if (!reader.isStartElement())
        {
          continue;
        }

        const auto tag = reader.name();

        // 2.0 says CoverageId, 1.1 says Identifier, 1.0 says name. All
        // three mean the string GetCoverage will ask for.
        if (tag == QLatin1String("CoverageId")
            || tag == QLatin1String("Identifier")
            || tag == QLatin1String("name"))
        {
          summary.identifier = reader.readElementText().trimmed();
        }
        else if (tag == QLatin1String("Title") || tag == QLatin1String("label"))
        {
          summary.title = reader.readElementText().trimmed();
        }
        else if (tag == QLatin1String("Abstract")
                 || tag == QLatin1String("description"))
        {
          summary.abstractText = reader.readElementText().trimmed();
        }
        else if (tag == QLatin1String("SupportedCRS"))
        {
          summary.supportedCrs.append(reader.readElementText().trimmed());
        }
        else if (tag == QLatin1String("SupportedFormat"))
        {
          summary.supportedFormats.append(reader.readElementText().trimmed());
        }
        else if (tag == QLatin1String("WGS84BoundingBox"))
        {
          summary.geographicBounds = readWgs84BoundingBox(reader);
        }
        else if (tag == QLatin1String("lonLatEnvelope"))
        {
          summary.geographicBounds = readLonLatEnvelope(reader);
        }
      }

      return summary;
    }
  } // namespace

  bool WcsCoverageDescription::isTwoDimensional() const
  {
    return axisLabels.size() == 2;
  }

  int WcsCoverageDescription::horizontalAxisIndex() const
  {
    for (int i = 0; i < axisLabels.size(); ++i)
    {
      const QString label = axisLabels.at(i).toLower();

      if (label == QLatin1String("lon") || label == QLatin1String("long")
          || label == QLatin1String("x") || label == QLatin1String("e")
          || label == QLatin1String("easting")
          || label == QLatin1String("longitude"))
      {
        return i;
      }
    }

    return -1;
  }

  int WcsCoverageDescription::verticalAxisIndex() const
  {
    for (int i = 0; i < axisLabels.size(); ++i)
    {
      const QString label = axisLabels.at(i).toLower();

      if (label == QLatin1String("lat") || label == QLatin1String("y")
          || label == QLatin1String("n") || label == QLatin1String("northing")
          || label == QLatin1String("latitude"))
      {
        return i;
      }
    }

    return -1;
  }

  QRectF WcsCoverageDescription::boundsAsRect() const
  {
    const int h = horizontalAxisIndex();
    const int v = verticalAxisIndex();

    if (h < 0 || v < 0 || h >= lowerCorner.size() || v >= lowerCorner.size()
        || h >= upperCorner.size() || v >= upperCorner.size())
    {
      return {};
    }

    return QRectF(QPointF(lowerCorner.at(h), lowerCorner.at(v)),
                  QPointF(upperCorner.at(h), upperCorner.at(v)))
      .normalized();
  }

  const WcsCoverageSummary *WcsCapabilities::coverage(
    const QString &identifier) const
  {
    for (const WcsCoverageSummary &summary : coverages)
    {
      if (summary.identifier == identifier)
      {
        return &summary;
      }
    }

    return nullptr;
  }

  WcsCapabilities parseWcsCapabilities(const QByteArray &xml)
  {
    WcsCapabilities capabilities;

    const QString exception = wcsExceptionText(xml);

    if (!exception.isEmpty())
    {
      capabilities.message = exception;
      return capabilities;
    }

    QXmlStreamReader reader(xml);
    bool sawRoot = false;

    while (!reader.atEnd())
    {
      reader.readNext();

      if (!reader.isStartElement())
      {
        continue;
      }

      const auto tag = reader.name();

      if (!sawRoot
          && (tag == QLatin1String("Capabilities")
              || tag == QLatin1String("WCS_Capabilities")))
      {
        sawRoot = true;

        // The version the answer declares, not the one that was asked
        // for: a server without the requested version answers in its
        // newest rather than refusing.
        capabilities.version =
          reader.attributes().value(QLatin1String("version")).toString();

        if (capabilities.version.isEmpty())
        {
          capabilities.version = tag == QLatin1String("WCS_Capabilities")
                                   ? QStringLiteral("1.0.0")
                                   : QStringLiteral("2.0.1");
        }

        continue;
      }

      if (!sawRoot)
      {
        continue;
      }

      if (capabilities.title.isEmpty()
          && (tag == QLatin1String("Title") || tag == QLatin1String("name")))
      {
        capabilities.title = reader.readElementText().trimmed();
      }
      else if (capabilities.abstractText.isEmpty()
               && tag == QLatin1String("Abstract"))
      {
        capabilities.abstractText = reader.readElementText().trimmed();
      }
      else if (tag == QLatin1String("CoverageSummary")
               || tag == QLatin1String("CoverageOfferingBrief"))
      {
        const WcsCoverageSummary summary =
          readCoverageSummary(reader, tag.toString());

        if (!summary.identifier.isEmpty())
        {
          capabilities.coverages.append(summary);
        }
      }
    }

    if (!sawRoot)
    {
      capabilities.message =
        QStringLiteral("The document is not a WCS capabilities response.");
      return capabilities;
    }

    capabilities.ok = true;

    return capabilities;
  }

  WcsCoverageDescription parseWcsCoverageDescription(const QByteArray &xml)
  {
    WcsCoverageDescription description;

    const QString exception = wcsExceptionText(xml);

    if (!exception.isEmpty())
    {
      description.message = exception;
      return description;
    }

    QXmlStreamReader reader(xml);
    bool sawDescription = false;
    bool done = false;

    while (!reader.atEnd() && !done)
    {
      reader.readNext();

      // The element is CoverageDescriptions, plural, and a server asked
      // about several coverages answers about all of them in one document.
      // This reads the first and stops: without the stop the second
      // coverage's envelope replaces the first's and its bands are
      // appended to the first's, so a two-coverage answer describes a
      // coverage that does not exist.
      if (reader.isEndElement()
          && reader.name() == QLatin1String("CoverageDescription"))
      {
        done = sawDescription;
        continue;
      }

      if (!reader.isStartElement())
      {
        continue;
      }

      const auto tag = reader.name();

      if (tag == QLatin1String("CoverageDescription"))
      {
        sawDescription = true;
        continue;
      }

      if (!sawDescription)
      {
        continue;
      }

      // The envelope's own labels. A description also carries the grid's
      // axisLabels, which name the grid's axes and are not what a subset
      // is written in.
      if (tag == QLatin1String("Envelope"))
      {
        const QXmlStreamAttributes attributes = reader.attributes();

        description.envelopeCrs =
          attributes.value(QLatin1String("srsName")).toString();

        description.axisLabels =
          attributes.value(QLatin1String("axisLabels"))
            .toString()
            .simplified()
            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
      }
      else if (tag == QLatin1String("lowerCorner"))
      {
        description.lowerCorner = readNumbers(reader.readElementText());
      }
      else if (tag == QLatin1String("upperCorner"))
      {
        description.upperCorner = readNumbers(reader.readElementText());
      }
      else if (tag == QLatin1String("CoverageId"))
      {
        description.identifier = reader.readElementText().trimmed();
      }
      else if (tag == QLatin1String("field"))
      {
        WcsRangeField field;

        field.name =
          reader.attributes().value(QLatin1String("name")).toString();

        // The description and the unit sit inside the field's quantity.
        int depth = 1;

        while (!reader.atEnd() && depth > 0)
        {
          reader.readNext();

          if (reader.isEndElement())
          {
            if (reader.name() == QLatin1String("field"))
            {
              --depth;
            }

            continue;
          }

          if (!reader.isStartElement())
          {
            continue;
          }

          if (reader.name() == QLatin1String("description"))
          {
            field.description = reader.readElementText().trimmed();
          }
          else if (reader.name() == QLatin1String("uom"))
          {
            field.uom =
              reader.attributes().value(QLatin1String("code")).toString();
          }
        }

        if (!field.name.isEmpty())
        {
          description.rangeFields.append(field);
        }
      }
    }

    if (!sawDescription)
    {
      description.message =
        QStringLiteral("The document is not a WCS coverage description.");
      return description;
    }

    description.ok = true;

    return description;
  }

  QString wcsExceptionText(const QByteArray &body)
  {
    QXmlStreamReader reader(body);
    QString code;
    QString text;
    bool isReport = false;

    while (!reader.atEnd())
    {
      reader.readNext();

      if (!reader.isStartElement())
      {
        continue;
      }

      const auto tag = reader.name();

      if (tag == QLatin1String("ExceptionReport")
          || tag == QLatin1String("ServiceExceptionReport"))
      {
        isReport = true;
      }
      else if (tag == QLatin1String("Exception")
               || tag == QLatin1String("ServiceException"))
      {
        if (code.isEmpty())
        {
          code = reader.attributes()
                   .value(QLatin1String("exceptionCode"))
                   .toString();
        }

        if (code.isEmpty())
        {
          code = reader.attributes().value(QLatin1String("code")).toString();
        }

        // 1.x writes the text as the element's own content; 2.0 puts it in
        // a child ExceptionText.
        const QString inline_ = reader.readElementText(
          QXmlStreamReader::IncludeChildElements).simplified();

        if (text.isEmpty())
        {
          text = inline_;
        }
      }
    }

    if (!isReport)
    {
      return {};
    }

    if (!code.isEmpty() && !text.isEmpty())
    {
      return QStringLiteral("%1: %2").arg(code, text);
    }

    if (!code.isEmpty())
    {
      return code;
    }

    return text.isEmpty() ? QStringLiteral("The service reported an error.")
                          : text;
  }

} // namespace HydroCouple::Ogc
