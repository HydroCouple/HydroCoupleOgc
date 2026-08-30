// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/wfsrequest.h"

#include "hydrocoupleogc/axisorder.h"

#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace HydroCouple::Ogc
{
  namespace
  {
    //! \returns \a value with enough digits for a metre-scale request.
    QString coordinate(double value)
    {
      return QString::number(value, 'f', 6);
    }

    //! Whether \a version is 2.0 or later, which renamed half the request.
    bool isTwoPointOh(const QString &version)
    {
      return version.startsWith(QLatin1String("2."));
    }
  }

  bool isGeoJsonFormat(const QString &format)
  {
    // Servers spell it "application/json", "application/json;
    // subtype=geojson", "geojson" and "json".
    return format.contains(QLatin1String("json"), Qt::CaseInsensitive);
  }

  QString preferredOutputFormat(const WfsFeatureType &type,
                                const QStringList &serviceFormats)
  {
    const QStringList offered =
      type.outputFormats.isEmpty() ? serviceFormats : type.outputFormats;

    // GeoJSON first: it reads without a schema and carries its own system,
    // where GML needs a driver and a schema fetch to know either.
    for (const QString &format : offered)
    {
      if (isGeoJsonFormat(format))
      {
        return format;
      }
    }

    for (const QString &format : offered)
    {
      if (format.contains(QLatin1String("gml"), Qt::CaseInsensitive))
      {
        return format;
      }
    }

    return {};
  }

  QString buildGetFeatureUrl(const WfsCapabilities &capabilities,
                             const WfsGetFeatureRequest &request)
  {
    if (capabilities.getFeatureUrl.isEmpty() || request.typeName.isEmpty()
        || request.count <= 0)
    {
      return {};
    }

    const QString version = capabilities.version.isEmpty()
                              ? QStringLiteral("2.0.0")
                              : capabilities.version;
    const bool twoPointOh = isTwoPointOh(version);

    QUrl url(capabilities.getFeatureUrl);
    QUrlQuery query(url.query());

    query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WFS"));
    query.addQueryItem(QStringLiteral("REQUEST"),
                       QStringLiteral("GetFeature"));
    query.addQueryItem(QStringLiteral("VERSION"), version);

    // Renamed in 2.0.0, and a server given the other spelling does not
    // refuse: it reports that no type was named at all.
    query.addQueryItem(twoPointOh ? QStringLiteral("TYPENAMES")
                                  : QStringLiteral("TYPENAME"),
                       request.typeName);

    // Likewise renamed -- and this one silently: a 1.1.0 server sent COUNT
    // ignores it and returns every feature it holds.
    query.addQueryItem(twoPointOh ? QStringLiteral("COUNT")
                                  : QStringLiteral("MAXFEATURES"),
                       QString::number(request.count));

    if (request.startIndex > 0)
    {
      // Paging arrived with 2.0.0. Before it, the only way to ask for less
      // is to ask for a smaller piece of ground.
      if (twoPointOh)
      {
        query.addQueryItem(QStringLiteral("STARTINDEX"),
                           QString::number(request.startIndex));
      }
    }

    if (!request.crs.isEmpty())
    {
      // One of the few that never changed name.
      query.addQueryItem(QStringLiteral("SRSNAME"), request.crs);
    }

    if (!request.extent.isNull())
    {
      const QRectF extent = request.extent.normalized();
      const double minimumX = extent.left();
      const double maximumX = extent.right();
      const double minimumY = std::min(extent.top(), extent.bottom());
      const double maximumY = std::max(extent.top(), extent.bottom());

      // Longitude first before 1.1.0, the authority's order from there on
      // -- the same rule WMS changed at 1.3.0, under a different number.
      const bool latitudeFirst =
        !version.startsWith(QLatin1String("1.0"))
        && authorityWritesLatitudeFirst(request.crs);

      QString box = latitudeFirst
                      ? QStringLiteral("%1,%2,%3,%4")
                          .arg(coordinate(minimumY), coordinate(minimumX),
                               coordinate(maximumY), coordinate(maximumX))
                      : QStringLiteral("%1,%2,%3,%4")
                          .arg(coordinate(minimumX), coordinate(minimumY),
                               coordinate(maximumX), coordinate(maximumY));

      // 2.0.0 lets the box name its own system, which takes the guesswork
      // out of the axis order for the server as well as for us.
      if (twoPointOh && !request.crs.isEmpty())
      {
        box += QLatin1Char(',') + request.crs;
      }

      query.addQueryItem(QStringLiteral("BBOX"), box);
    }

    if (!request.outputFormat.isEmpty())
    {
      query.addQueryItem(QStringLiteral("OUTPUTFORMAT"),
                         request.outputFormat);
    }

    url.setQuery(query);

    return url.toString();
  }

} // namespace HydroCouple::Ogc
