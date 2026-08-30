// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   httpclient.h
 * \author Caleb Buahin
 * \brief  Fetching over HTTP, once per thing rather than once per asker.
 *
 * GDAL is not used to fetch anything anywhere in this library: in both
 * projects that consume it GDAL is built without curl, so its WMS, WCS and
 * WFS drivers are not registered at all and CPLHTTPFetch is a stub that
 * returns "not compiled with libcurl support". It can still decode bytes,
 * which is what it is used for; it cannot obtain them.
 */

#ifndef HYDROCOUPLEOGC_HTTPCLIENT_H
#define HYDROCOUPLEOGC_HTTPCLIENT_H

#include "hydrocoupleogc/export.h"
#include "hydrocoupleogc/servicecredentials.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace HydroCouple::Ogc
{
  /*!
   * \brief What came back.
   */
  struct HYDROCOUPLEOGC_EXPORT HttpResponse
  {
      //! Whether the body is the thing that was asked for.
      bool ok = false;

      //! The HTTP status, or 0 when the request never reached a server.
      int statusCode = 0;

      QString contentType;

      /*!
       * \brief The body, delivered whether or not the request succeeded.
       *
       * A refusal from an OGC service is usually an XML document saying
       * why, and it arrives under a 200 as often as under a 400. Throwing
       * the body away on failure — as a client that checks the status
       * first would — turns "layer FOO is not published here" into
       * "Bad Request".
       */
      QByteArray body;

      //! Why not, when \a ok is false.
      QString error;

      //! What was finally fetched, after any redirects.
      QUrl url;
  };

  /*!
   * \brief One network access manager, shared, with the bookkeeping around
   *        it that a tiled map needs.
   *
   * Panning a map asks for the same tile from several places at once and
   * then abandons most of what it asked for. Two things follow, and both
   * live here rather than in each layer that would otherwise repeat them:
   * a second ask for something already in flight waits on the first
   * request rather than making another, and a caller that gives up
   * releases its claim without disturbing the callers that did not.
   */
  class HYDROCOUPLEOGC_EXPORT HttpClient : public QObject
  {
      Q_OBJECT

    public:
      //! Called on the thread the client lives on, once, per get().
      using Callback = std::function<void(const HttpResponse &)>;

      //! What get() returns, and cancel() takes. Never 0 for a live ask.
      using RequestId = quint64;

      /*!
       * \brief Constructs a client with nothing outstanding.
       *
       * Destroying one abandons whatever it was fetching and calls none of
       * the callbacks, which is what a layer being closed mid-pan needs —
       * and follows from the manager being a child of the client and the
       * callbacks being bound to it, so it needs no destructor of its own.
       */
      explicit HttpClient(QObject *parent = nullptr);

      /*!
       * \brief Where responses are cached between sessions.
       *
       * Off until set: a library that starts writing to a user's disk
       * because it was linked has overstepped. Capabilities documents and
       * tiles both carry cache headers worth honouring, and honouring them
       * is what keeps a re-opened project from re-fetching a basemap.
       */
      void setCacheDirectory(const QString &path, qint64 maximumBytes = 0);

      //! Sent on every request; identifies the client to a server's logs.
      void setUserAgent(const QString &userAgent);

      //! How long a request may stall before it is given up on.
      void setTransferTimeout(int milliseconds);

      /*!
       * \brief Asks for \a url, calling \a callback when it is known.
       *
       * When an identical ask is already in flight — the same URL under
       * the same credentials — no second request is made and both callers
       * are answered from the one response.
       *
       * \returns An id to cancel by, or 0 when \a url cannot be fetched.
       */
      RequestId get(const QUrl &url, const ServiceCredentials &credentials,
                    Callback callback);

      //! \overload
      RequestId get(const QUrl &url, Callback callback);

      /*!
       * \brief Gives up on \a id.
       *
       * The callback will not be called. The request itself is abandoned
       * only when no other caller is still waiting on it.
       */
      void cancel(RequestId id);

      //! Gives up on everything outstanding.
      void cancelAll();

      //! How many callers are waiting.
      [[nodiscard]] int pendingCount() const;

      /*!
       * \brief How many requests are actually on the wire.
       *
       * Less than pendingCount() exactly when asks have been shared, which
       * is the whole point of sharing them.
       */
      [[nodiscard]] int inFlightCount() const;

    private:
      struct Waiter
      {
          RequestId id = 0;
          QByteArray key;
          Callback callback;
      };

      struct InFlight
      {
          QNetworkReply *reply = nullptr;
          QList<RequestId> waiters;
      };

      void deliver(const QByteArray &key, const HttpResponse &response);

      QNetworkAccessManager *m_manager = nullptr;
      QString m_userAgent;
      int m_transferTimeout = 30000;
      RequestId m_nextId = 1;

      QHash<RequestId, Waiter> m_waiters;
      QHash<QByteArray, InFlight> m_inFlight;
  };

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_HTTPCLIENT_H
