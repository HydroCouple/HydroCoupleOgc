// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/servicediscovery.h"

#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace HydroCouple::Ogc
{
  QString buildCapabilitiesUrl(const QString &serviceUrl, ServiceKind kind)
  {
    if (serviceUrl.trimmed().isEmpty() || kind == ServiceKind::Unknown)
    {
      return {};
    }

    QUrl url(serviceUrl.trimmed());

    if (!url.isValid() || url.host().isEmpty())
    {
      return {};
    }

    if (url.scheme().isEmpty())
    {
      url.setScheme(QStringLiteral("https"));
    }

    const QUrlQuery existing(url.query());
    QUrlQuery query;

    // Whatever else the address carries is kept; only the three parameters
    // that say what is being asked are this library's to set.
    const QList<QPair<QString, QString>> items = existing.queryItems();

    for (const QPair<QString, QString> &item : items)
    {
      const QString name = item.first.toUpper();

      if (name == QLatin1String("SERVICE") || name == QLatin1String("REQUEST")
          || name == QLatin1String("VERSION"))
      {
        continue;
      }

      query.addQueryItem(item.first, item.second);
    }

    query.addQueryItem(QStringLiteral("SERVICE"),
                       kind == ServiceKind::Wms ? QStringLiteral("WMS")
                                                : QStringLiteral("WMTS"));
    query.addQueryItem(QStringLiteral("REQUEST"),
                       QStringLiteral("GetCapabilities"));

    // The newest version each service has. A server that does not have it
    // answers in the newest it does have, and everything downstream follows
    // the answer.
    query.addQueryItem(QStringLiteral("VERSION"),
                       kind == ServiceKind::Wms ? QStringLiteral("1.3.0")
                                                : QStringLiteral("1.0.0"));

    url.setQuery(query);

    return url.toString();
  }

  ServiceKind detectServiceKind(const QByteArray &xml)
  {
    QXmlStreamReader reader(xml);

    while (!reader.atEnd())
    {
      reader.readNext();

      if (!reader.isStartElement())
      {
        continue;
      }

      const QString element = reader.name().toString();

      if (element == QLatin1String("WMS_Capabilities")
          || element == QLatin1String("WMT_MS_Capabilities"))
      {
        return ServiceKind::Wms;
      }

      if (element == QLatin1String("Capabilities"))
      {
        return ServiceKind::Wmts;
      }

      // Read on rather than deciding on the first element: WMTS may be
      // encoded in a SOAP envelope, whose body holds the very document the
      // parsers here already accept. Nothing is lost by looking, because
      // the documents that must NOT be taken for a service -- an exception
      // report, a sign-in page -- contain no element by these names.
    }

    return ServiceKind::Unknown;
  }

} // namespace HydroCouple::Ogc
