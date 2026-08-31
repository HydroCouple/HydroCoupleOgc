// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/wcsrequest.h"

#include "hydrocoupleogc/crsurn.h"

#include <QUrl>
#include <QUrlQuery>

namespace HydroCouple::Ogc
{
  namespace
  {
    //! Six decimals: about 0.1 m in degrees, well under any coverage's cell.
    QString number(double value)
    {
      return QString::number(value, 'f', 6);
    }

    /*!
     * \brief Starts a query with the three parameters every request carries.
     */
    QUrlQuery baseQuery(const QString &version, const QString &request)
    {
      QUrlQuery query;

      query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WCS"));
      query.addQueryItem(QStringLiteral("VERSION"), version);
      query.addQueryItem(QStringLiteral("REQUEST"), request);

      return query;
    }

    QString finish(const QString &serviceUrl, const QUrlQuery &query)
    {
      QUrl url(serviceUrl.trimmed());

      if (!url.isValid() || url.host().isEmpty())
      {
        return {};
      }

      url.setQuery(query);

      return url.toString();
    }

    //! The urn form 1.1 writes its CRS references in.
    QString crsUrn(const QString &crs)
    {
      const CrsIdentifier id = parseCrsIdentifier(crs);

      if (id.authority.isEmpty() || id.code.isEmpty())
      {
        return crs;
      }

      return QStringLiteral("urn:ogc:def:crs:%1::%2").arg(id.authority, id.code);
    }

    /*!
     * \brief Writes the 2.0 form, which subsets each axis by its own name.
     */
    bool addTwoZeroSubsets(QUrlQuery &query,
                           const WcsGetCoverageRequest &request,
                           const WcsCoverageDescription &description)
    {
      const int h = description.horizontalAxisIndex();
      const int v = description.verticalAxisIndex();

      if (h < 0 || v < 0)
      {
        return false;
      }

      const QString horizontal = description.axisLabels.at(h);
      const QString vertical = description.axisLabels.at(v);

      // By name, so the order these are written in carries no meaning and
      // an axis-order convention cannot be got wrong here. What can be got
      // wrong is the name.
      query.addQueryItem(QStringLiteral("SUBSET"),
                         QStringLiteral("%1(%2,%3)")
                           .arg(horizontal, number(request.extent.left()),
                                number(request.extent.right())));

      query.addQueryItem(QStringLiteral("SUBSET"),
                         QStringLiteral("%1(%2,%3)")
                           .arg(vertical, number(request.extent.top()),
                                number(request.extent.bottom())));

      if (!request.size.isEmpty())
      {
        query.addQueryItem(
          QStringLiteral("SCALESIZE"),
          QStringLiteral("%1(%2),%3(%4)")
            .arg(horizontal, QString::number(request.size.width()), vertical,
                 QString::number(request.size.height())));
      }

      return true;
    }
  } // namespace

  QString buildGetCoverageUrl(const QString &serviceUrl,
                              const QString &version,
                              const WcsGetCoverageRequest &request,
                              const WcsCoverageDescription &description)
  {
    if (request.coverageId.isEmpty() || request.extent.isNull())
    {
      return {};
    }

    QUrlQuery query = baseQuery(version, QStringLiteral("GetCoverage"));

    const QRectF extent = request.extent.normalized();
    WcsGetCoverageRequest normalised = request;
    normalised.extent = extent;

    if (version.startsWith(QLatin1String("2")))
    {
      query.addQueryItem(QStringLiteral("COVERAGEID"), request.coverageId);
      query.addQueryItem(QStringLiteral("FORMAT"), request.format);

      if (!addTwoZeroSubsets(query, normalised, description))
      {
        // Refused, not guessed. See the header: an invented axis label is
        // answered with an error status that reads like a missing endpoint.
        return {};
      }

      if (!request.subsettingCrs.isEmpty())
      {
        query.addQueryItem(QStringLiteral("SUBSETTINGCRS"),
                           request.subsettingCrs);
      }

      if (!request.outputCrs.isEmpty())
      {
        query.addQueryItem(QStringLiteral("OUTPUTCRS"), request.outputCrs);
      }

      if (!request.rangeSubset.isEmpty())
      {
        query.addQueryItem(QStringLiteral("RANGESUBSET"),
                           request.rangeSubset.join(QLatin1Char(',')));
      }

      return finish(serviceUrl, query);
    }

    if (version.startsWith(QLatin1String("1.1")))
    {
      const QString crs = crsUrn(request.subsettingCrs);

      query.addQueryItem(QStringLiteral("IDENTIFIER"), request.coverageId);
      query.addQueryItem(QStringLiteral("FORMAT"), request.format);

      // 1.1's bounding box names its own system as a fifth element, the
      // same shape a WFS 1.1 bbox uses.
      query.addQueryItem(QStringLiteral("BOUNDINGBOX"),
                         QStringLiteral("%1,%2,%3,%4,%5")
                           .arg(number(extent.left()), number(extent.top()),
                                number(extent.right()), number(extent.bottom()),
                                crs));

      if (!request.rangeSubset.isEmpty())
      {
        query.addQueryItem(QStringLiteral("RANGESUBSET"),
                           request.rangeSubset.join(QLatin1Char(',')));
      }

      return finish(serviceUrl, query);
    }

    query.addQueryItem(QStringLiteral("COVERAGE"), request.coverageId);
    query.addQueryItem(QStringLiteral("FORMAT"), request.format);
    query.addQueryItem(QStringLiteral("CRS"), request.subsettingCrs);
    query.addQueryItem(QStringLiteral("BBOX"),
                       QStringLiteral("%1,%2,%3,%4")
                         .arg(number(extent.left()), number(extent.top()),
                              number(extent.right()), number(extent.bottom())));

    if (!request.size.isEmpty())
    {
      query.addQueryItem(QStringLiteral("WIDTH"),
                         QString::number(request.size.width()));
      query.addQueryItem(QStringLiteral("HEIGHT"),
                         QString::number(request.size.height()));
    }

    return finish(serviceUrl, query);
  }

  QString buildDescribeCoverageUrl(const QString &serviceUrl,
                                   const QString &version,
                                   const QString &coverageId)
  {
    if (coverageId.isEmpty())
    {
      return {};
    }

    QUrlQuery query = baseQuery(version, QStringLiteral("DescribeCoverage"));

    // The parameter is named for what the version calls a coverage, the
    // same split the GetCoverage forms follow.
    if (version.startsWith(QLatin1String("2")))
    {
      query.addQueryItem(QStringLiteral("COVERAGEID"), coverageId);
    }
    else if (version.startsWith(QLatin1String("1.1")))
    {
      query.addQueryItem(QStringLiteral("IDENTIFIERS"), coverageId);
    }
    else
    {
      query.addQueryItem(QStringLiteral("COVERAGE"), coverageId);
    }

    return finish(serviceUrl, query);
  }

  QString nextWcsVersion(const QString &version)
  {
    if (version.startsWith(QLatin1String("2")))
    {
      return QStringLiteral("1.1.2");
    }

    if (version.startsWith(QLatin1String("1.1")))
    {
      return QStringLiteral("1.0.0");
    }

    return {};
  }

} // namespace HydroCouple::Ogc
