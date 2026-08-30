// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/wmsrequest.h"

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
  }

  bool wmsAxisOrderIsLatitudeFirst(const QString &crs, const QString &version)
  {
    // Before 1.3.0 every bounding box is longitude first, whatever system it
    // is in. The rule changed with the version, which is why the version the
    // *document* declares is the one that matters.
    if (!version.startsWith(QLatin1String("1.3")))
    {
      return false;
    }

    return authorityWritesLatitudeFirst(crs);
  }

  QString buildGetMapUrl(const WmsCapabilities &capabilities,
                         const WmsGetMapRequest &request)
  {
    if (capabilities.getMapUrl.isEmpty() || request.layers.isEmpty()
        || request.crs.isEmpty() || request.widthPixels <= 0
        || request.heightPixels <= 0)
    {
      return {};
    }

    const QString version = capabilities.version.isEmpty()
                              ? QStringLiteral("1.3.0")
                              : capabilities.version;

    QString format = request.format;

    if (format.isEmpty())
    {
      format = capabilities.imageFormats.isEmpty()
                 ? QStringLiteral("image/png")
                 : capabilities.imageFormats.first();
    }

    const QRectF extent = request.extent.normalized();
    const double minimumX = extent.left();
    const double maximumX = extent.right();
    const double minimumY = std::min(extent.top(), extent.bottom());
    const double maximumY = std::max(extent.top(), extent.bottom());

    const bool latitudeFirst =
      wmsAxisOrderIsLatitudeFirst(request.crs, version);

    const QString boundingBox =
      latitudeFirst ? QStringLiteral("%1,%2,%3,%4")
                        .arg(coordinate(minimumY), coordinate(minimumX),
                             coordinate(maximumY), coordinate(maximumX))
                    : QStringLiteral("%1,%2,%3,%4")
                        .arg(coordinate(minimumX), coordinate(minimumY),
                             coordinate(maximumX), coordinate(maximumY));

    QUrl url(capabilities.getMapUrl);
    QUrlQuery query(url.query());

    query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WMS"));
    query.addQueryItem(QStringLiteral("REQUEST"), QStringLiteral("GetMap"));
    query.addQueryItem(QStringLiteral("VERSION"), version);
    query.addQueryItem(QStringLiteral("LAYERS"),
                       request.layers.join(QLatin1Char(',')));

    // STYLES is not optional even when it is empty: a server may answer a
    // request that omits it, and many refuse.
    query.addQueryItem(QStringLiteral("STYLES"),
                       request.styles.join(QLatin1Char(',')));

    query.addQueryItem(version.startsWith(QLatin1String("1.3"))
                         ? QStringLiteral("CRS")
                         : QStringLiteral("SRS"),
                       request.crs);

    query.addQueryItem(QStringLiteral("BBOX"), boundingBox);
    query.addQueryItem(QStringLiteral("WIDTH"),
                       QString::number(request.widthPixels));
    query.addQueryItem(QStringLiteral("HEIGHT"),
                       QString::number(request.heightPixels));
    query.addQueryItem(QStringLiteral("FORMAT"), format);
    query.addQueryItem(QStringLiteral("TRANSPARENT"),
                       request.transparent ? QStringLiteral("TRUE")
                                           : QStringLiteral("FALSE"));

    if (!request.backgroundColour.isEmpty())
    {
      query.addQueryItem(QStringLiteral("BGCOLOR"), request.backgroundColour);
    }

    url.setQuery(query);

    return url.toString();
  }

} // namespace HydroCouple::Ogc
