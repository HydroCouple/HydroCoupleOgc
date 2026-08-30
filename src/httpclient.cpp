// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/httpclient.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <algorithm>

namespace HydroCouple::Ogc
{
  namespace
  {
    /*!
     * \brief What decides whether two asks are the same ask.
     *
     * The URL and who is asking, because a request carrying credentials
     * and one carrying none are answered differently by the same server
     * and sharing them across would hand one caller the other's access.
     */
    QByteArray shareKey(const QUrl &url, const ServiceCredentials &credentials)
    {
      return url.toEncoded() + '\n' + credentials.fingerprint();
    }
  }

  HttpClient::HttpClient(QObject *parent)
    : QObject(parent),
      m_manager(new QNetworkAccessManager(this)),
      m_userAgent(QStringLiteral("HydroCoupleOgc/0.1 (hydrocouple.org)"))
  {
    // Follow redirects, but not off the host that was asked for: a service
    // URL that redirects to a login page should fail as a service rather
    // than succeed as a page.
    m_manager->setRedirectPolicy(
      QNetworkRequest::SameOriginRedirectPolicy);
  }

  void HttpClient::setCacheDirectory(const QString &path, qint64 maximumBytes)
  {
    if (path.isEmpty())
    {
      m_manager->setCache(nullptr);

      return;
    }

    QDir().mkpath(path);

    auto *cache = new QNetworkDiskCache(m_manager);
    cache->setCacheDirectory(path);

    if (maximumBytes > 0)
    {
      cache->setMaximumCacheSize(maximumBytes);
    }

    // Takes ownership, and deletes whatever was there before.
    m_manager->setCache(cache);
  }

  void HttpClient::setUserAgent(const QString &userAgent)
  {
    m_userAgent = userAgent;
  }

  void HttpClient::setTransferTimeout(int milliseconds)
  {
    m_transferTimeout = milliseconds;
  }

  HttpClient::RequestId HttpClient::get(const QUrl &url, Callback callback)
  {
    return get(url, ServiceCredentials{}, std::move(callback));
  }

  HttpClient::RequestId HttpClient::get(const QUrl &url,
                                        const ServiceCredentials &credentials,
                                        Callback callback)
  {
    if (!url.isValid() || url.scheme().isEmpty() || !callback)
    {
      return 0;
    }

    const QByteArray key = shareKey(url, credentials);
    const RequestId id = m_nextId++;

    m_waiters.insert(id, Waiter{id, key, std::move(callback)});

    const auto existing = m_inFlight.find(key);

    if (existing != m_inFlight.end())
    {
      // Somebody already asked for this. Wait on their answer.
      existing->waiters.append(id);

      return id;
    }

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", m_userAgent.toUtf8());
    request.setTransferTimeout(m_transferTimeout);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
    credentials.applyTo(request);

    qCDebug(ogcHttp) << "GET" << redactedUrl(url);

    QNetworkReply *reply = m_manager->get(request);

    m_inFlight.insert(key, InFlight{reply, {id}});

    connect(reply, &QNetworkReply::finished, this, [this, key, reply]() {
      reply->deleteLater();

      // The entry is taken out before anything is delivered, so that a
      // callback which immediately asks for the same URL again starts a
      // new request rather than joining one that has already finished.
      const auto entry = m_inFlight.find(key);

      if (entry == m_inFlight.end() || entry->reply != reply)
      {
        // Cancelled, and this is the abort arriving. Nobody is waiting.
        return;
      }

      m_inFlight.erase(entry);

      HttpResponse response;
      response.url = reply->url();
      response.statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      response.contentType =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();

      // Read the body first and unconditionally: a service that refuses a
      // request says why in the body, under a 200 as often as under a 400.
      response.body = reply->readAll();

      // A status of 400 or worse is already a reply error here -- Qt maps
      // it to one -- so the status is reported rather than judged.
      if (reply->error() != QNetworkReply::NoError)
      {
        response.error = reply->errorString();
      }
      else
      {
        response.ok = true;
      }

      if (!response.ok)
      {
        qCDebug(ogcHttp) << "failed" << redactedUrl(reply->url())
                         << response.error;
      }

      deliver(key, response);
    });

    return id;
  }

  void HttpClient::deliver(const QByteArray &key, const HttpResponse &response)
  {
    // A callback may cancel other requests, so the list is taken by value
    // and each waiter looked up again as it is reached.
    QList<RequestId> waiting;

    for (auto it = m_waiters.cbegin(); it != m_waiters.cend(); ++it)
    {
      if (it->key == key)
      {
        waiting.append(it.key());
      }
    }

    std::sort(waiting.begin(), waiting.end());

    for (const RequestId id : waiting)
    {
      const auto waiter = m_waiters.find(id);

      if (waiter == m_waiters.end())
      {
        continue;
      }

      const Callback callback = waiter->callback;
      m_waiters.erase(waiter);

      callback(response);
    }
  }

  void HttpClient::cancel(RequestId id)
  {
    const auto waiter = m_waiters.find(id);

    if (waiter == m_waiters.end())
    {
      return;
    }

    const QByteArray key = waiter->key;
    m_waiters.erase(waiter);

    const auto entry = m_inFlight.find(key);

    if (entry == m_inFlight.end())
    {
      return;
    }

    entry->waiters.removeAll(id);

    if (!entry->waiters.isEmpty())
    {
      // Somebody else still wants it.
      return;
    }

    QNetworkReply *reply = entry->reply;

    // Out of the table BEFORE abort(). abort() can emit finished()
    // synchronously, from inside this call, and a handler that then found
    // this entry still present would deliver a response to callers who
    // have just withdrawn — or, once the entry is erased under it, read
    // freed memory. This ordering is the fix for a real crash.
    m_inFlight.erase(entry);

    if (reply)
    {
      reply->abort();
    }
  }

  void HttpClient::cancelAll()
  {
    // Every request has at least one caller waiting on it, so withdrawing
    // all of them abandons all of them.
    const QList<RequestId> ids = m_waiters.keys();

    for (const RequestId id : ids)
    {
      cancel(id);
    }
  }

  int HttpClient::pendingCount() const
  {
    return int(m_waiters.size());
  }

  int HttpClient::inFlightCount() const
  {
    return int(m_inFlight.size());
  }

} // namespace HydroCouple::Ogc
