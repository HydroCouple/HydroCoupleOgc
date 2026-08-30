// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/servicecredentials.h"

#include <QCryptographicHash>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace HydroCouple::Ogc
{
  // Warnings and worse by default; the request log is turned on with
  // QT_LOGGING_RULES="hydrocouple.ogc.http.debug=true". The implementation
  // this replaces logged every request unconditionally through qDebug().
  Q_LOGGING_CATEGORY(ogcHttp, "hydrocouple.ogc.http", QtWarningMsg)

  namespace
  {
    /*!
     * \brief Whether a query item's name suggests it carries a secret.
     *
     * Matched loosely and on purpose: the cost of redacting a harmless
     * parameter is an unreadable log line, and the cost of missing one is
     * a key in a file that outlives the session.
     */
    bool namesASecret(const QString &key)
    {
      static const QStringList markers = {
        QStringLiteral("key"),    QStringLiteral("token"),
        QStringLiteral("secret"), QStringLiteral("password"),
        QStringLiteral("passwd"), QStringLiteral("signature"),
        QStringLiteral("sig"),    QStringLiteral("auth"),
        QStringLiteral("access")};

      for (const QString &marker : markers)
      {
        if (key.contains(marker, Qt::CaseInsensitive))
        {
          return true;
        }
      }

      return false;
    }
  }

  void ServiceCredentials::applyTo(QNetworkRequest &request) const
  {
    if (!username.isEmpty() || !password.isEmpty())
    {
      const QByteArray pair =
        (username + QLatin1Char(':') + password).toUtf8().toBase64();

      request.setRawHeader("Authorization", "Basic " + pair);
    }

    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
    {
      if (it.value().isEmpty())
      {
        continue;
      }

      // Sent under the name it is stored under, including the "referer"
      // that QGIS-style settings spell in lower case: header names are
      // case-insensitive on the wire, so nothing is gained by respelling
      // it and a server gating on the Referer sees it either way.
      request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }
  }

  QByteArray ServiceCredentials::fingerprint() const
  {
    QCryptographicHash hash(QCryptographicHash::Sha256);

    hash.addData(username.toUtf8());
    hash.addData("\n");
    hash.addData(password.toUtf8());

    // A QMap iterates in key order, so the same headers written in a
    // different order fingerprint the same and their requests are shared.
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
    {
      hash.addData("\n");
      hash.addData(it.key().toUtf8());
      hash.addData("\t");
      hash.addData(it.value().toUtf8());
    }

    return hash.result().toHex();
  }

  QString redactedUrl(const QUrl &url)
  {
    QUrl copy = url;

    copy.setUserInfo(QString());

    const QUrlQuery query(copy);

    if (!query.isEmpty())
    {
      QUrlQuery redacted;

      const QList<QPair<QString, QString>> items = query.queryItems();

      for (const QPair<QString, QString> &item : items)
      {
        redacted.addQueryItem(item.first, namesASecret(item.first)
                                            ? QStringLiteral("[redacted]")
                                            : item.second);
      }

      copy.setQuery(redacted);
    }

    return copy.toString();
  }

} // namespace HydroCouple::Ogc
