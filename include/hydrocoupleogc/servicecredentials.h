// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   servicecredentials.h
 * \author Caleb Buahin
 * \brief  What a request has to carry to be let through, and how to say so
 *         in a log without saying it.
 */

#ifndef HYDROCOUPLEOGC_SERVICECREDENTIALS_H
#define HYDROCOUPLEOGC_SERVICECREDENTIALS_H

#include "hydrocoupleogc/export.h"

#include <QLoggingCategory>
#include <QMap>
#include <QString>
#include <QUrl>

class QNetworkRequest;

namespace HydroCouple::Ogc
{
  //! Everything this library logs, off unless asked for.
  HYDROCOUPLEOGC_EXPORT Q_DECLARE_LOGGING_CATEGORY(ogcHttp)

  /*!
   * \brief What a service needs before it will answer.
   *
   * Held in memory for as long as a layer is open and never written to a
   * project file: a project is shared, and a service password is not.
   */
  struct HYDROCOUPLEOGC_EXPORT ServiceCredentials
  {
      QString username;
      QString password;

      /*!
       * \brief Headers to send, keyed by header name.
       *
       * Sent as written; HTTP header names are case-insensitive, so a
       * "referer" stored in lower case reaches a server gating on the
       * Referer header unchanged. QGIS stores the same map under a
       * `http-header/` settings group, and openswmm.gui followed it, so
       * connection files exported from any of the three interchange.
       */
      QMap<QString, QString> headers;

      [[nodiscard]] bool isEmpty() const
      {
        return username.isEmpty() && password.isEmpty() && headers.isEmpty();
      }

      /*!
       * \brief Puts these on \a request.
       *
       * Basic authentication is written as a header rather than left to
       * QNetworkAccessManager's authenticationRequired signal, because that
       * signal costs a round trip per request and fires on a thread the
       * caller may not be on.
       */
      void applyTo(QNetworkRequest &request) const;

      /*!
       * \brief A stable fingerprint of these credentials.
       *
       * Requests are only shared between callers when they agree, and this
       * is what "agree" is decided on. It is a digest rather than the
       * values themselves so that no map anywhere in this library is keyed
       * by a password.
       */
      [[nodiscard]] QByteArray fingerprint() const;
  };

  /*!
   * \brief \a url with anything secret in it replaced.
   *
   * Half of these services are addressed with the key in the query string,
   * so logging a request URL as it stands publishes the key to anyone who
   * later reads the log — which is exactly what the implementation this
   * replaces did, unconditionally and at every zoom. Userinfo goes, and so
   * does the value of any query item whose name looks like a secret.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString redactedUrl(const QUrl &url);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_SERVICECREDENTIALS_H
