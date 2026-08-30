// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   test_httpclient.cpp
 * \brief  Fetching, sharing and giving up.
 *
 * These gates talk HTTP to a server that runs inside the test process and
 * listens on the loopback interface. Nothing here reaches the internet: a
 * test that depended on a public server would be a test that fails on a
 * train, and the behaviour worth gating — that one ask is made once, that a
 * caller can withdraw without disturbing the others, that a refusal still
 * carries what the server said — is about this client and not about any
 * particular server.
 *
 * The server also lets a response be held open on demand, which is what
 * makes the sharing and cancellation gates deterministic rather than
 * dependent on winning a race.
 */

#include "hydrocoupleogc/httpclient.h"
#include "hydrocoupleogc/servicecredentials.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

using namespace HydroCouple::Ogc;

namespace
{
  /*!
   * \brief What the server was asked for.
   */
  struct RecordedRequest
  {
      QByteArray method;
      QByteArray path;
      QMap<QByteArray, QByteArray> headers;
  };

  /*!
   * \brief An HTTP server, in the test process, on the loopback interface.
   */
  class LocalServer : public QTcpServer
  {
    public:
      explicit LocalServer(QObject *parent = nullptr) : QTcpServer(parent)
      {
        listen(QHostAddress::LocalHost, 0);
      }

      QUrl urlFor(const QString &path) const
      {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                      .arg(serverPort())
                      .arg(path));
      }

      //! What to answer with. Held responses wait for release().
      void reply(int status, const QByteArray &contentType,
                 const QByteArray &body,
                 const QByteArray &extraHeaders = QByteArray())
      {
        m_status = status;
        m_contentType = contentType;
        m_body = body;
        m_extraHeaders = extraHeaders;
      }

      //! Answers are withheld until release() is called.
      void hold() { m_held = true; }

      void release()
      {
        m_held = false;

        for (QTcpSocket *socket : std::as_const(m_waiting))
        {
          answer(socket);
        }

        m_waiting.clear();
      }

      [[nodiscard]] int requestCount() const { return m_requests.size(); }

      [[nodiscard]] const QList<RecordedRequest> &requests() const
      {
        return m_requests;
      }

      //! Sockets closed by the client rather than answered.
      [[nodiscard]] int abandonedCount() const { return m_abandoned; }

    protected:
      void incomingConnection(qintptr handle) override
      {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        QObject::connect(socket, &QTcpSocket::readyRead, socket,
                         [this, socket]() { onReadyRead(socket); });

        QObject::connect(socket, &QTcpSocket::disconnected, socket,
                         [this, socket]() {
                           if (m_waiting.removeAll(socket) > 0)
                           {
                             ++m_abandoned;
                           }

                           socket->deleteLater();
                         });
      }

    private:
      void onReadyRead(QTcpSocket *socket)
      {
        m_buffers[socket].append(socket->readAll());

        if (!m_buffers[socket].contains("\r\n\r\n"))
        {
          return;
        }

        const QByteArray head = m_buffers.take(socket);
        const QList<QByteArray> lines = head.split('\n');

        RecordedRequest request;

        if (!lines.isEmpty())
        {
          const QList<QByteArray> parts = lines.first().simplified().split(' ');

          if (parts.size() >= 2)
          {
            request.method = parts.at(0);
            request.path = parts.at(1);
          }
        }

        for (int index = 1; index < lines.size(); ++index)
        {
          const QByteArray line = lines.at(index).trimmed();
          const int colon = line.indexOf(':');

          if (colon > 0)
          {
            request.headers.insert(line.left(colon).toLower(),
                                   line.mid(colon + 1).trimmed());
          }
        }

        m_requests.append(request);

        if (m_held)
        {
          m_waiting.append(socket);

          return;
        }

        answer(socket);
      }

      void answer(QTcpSocket *socket)
      {
        if (socket->state() != QAbstractSocket::ConnectedState)
        {
          return;
        }

        QByteArray response = "HTTP/1.1 " + QByteArray::number(m_status)
                              + " Response\r\n";
        response += "Content-Type: " + m_contentType + "\r\n";
        response += "Content-Length: " + QByteArray::number(m_body.size())
                    + "\r\n";
        response += m_extraHeaders;
        response += "Connection: close\r\n\r\n";
        response += m_body;

        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
      }

      int m_status = 200;
      QByteArray m_contentType = "text/plain";
      QByteArray m_body = "ok";
      QByteArray m_extraHeaders;
      bool m_held = false;
      int m_abandoned = 0;

      QList<RecordedRequest> m_requests;
      QList<QTcpSocket *> m_waiting;
      QMap<QTcpSocket *, QByteArray> m_buffers;
  };

  //! Spins the event loop until \a done, or until \a milliseconds pass.
  bool waitFor(const std::function<bool()> &done, int milliseconds = 4000)
  {
    QElapsedTimer timer;
    timer.start();

    while (!done() && timer.elapsed() < milliseconds)
    {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }

    return done();
  }
}

// ── what comes back ─────────────────────────────────────────────────────────

TEST(HttpClientTest, AnAnswerCarriesItsBodyAndItsType)
{
  LocalServer server;
  server.reply(200, "application/xml", "<Capabilities/>");

  HttpClient client;
  HttpResponse got;
  bool called = false;

  const auto id = client.get(server.urlFor(QStringLiteral("/wms")),
                             [&](const HttpResponse &response) {
                               got = response;
                               called = true;
                             });

  ASSERT_NE(id, 0u);
  ASSERT_TRUE(waitFor([&] { return called; }));

  EXPECT_TRUE(got.ok) << got.error.toStdString();
  EXPECT_EQ(got.statusCode, 200);
  EXPECT_EQ(got.body, QByteArray("<Capabilities/>"));
  EXPECT_TRUE(got.contentType.contains(QStringLiteral("xml")));
  EXPECT_EQ(client.pendingCount(), 0);
  EXPECT_EQ(client.inFlightCount(), 0);
}

TEST(HttpClientTest, ARefusalStillCarriesWhatTheServerSaid)
{
  LocalServer server;
  server.reply(400, "application/xml",
               "<ServiceExceptionReport><ServiceException>Layer FOO is not "
               "published here</ServiceException></ServiceExceptionReport>");

  HttpClient client;
  HttpResponse got;
  bool called = false;

  client.get(server.urlFor(QStringLiteral("/wms")),
             [&](const HttpResponse &response) {
               got = response;
               called = true;
             });

  ASSERT_TRUE(waitFor([&] { return called; }));

  EXPECT_FALSE(got.ok);
  EXPECT_EQ(got.statusCode, 400);

  // The whole reason to keep the body on a failure: the server named the
  // problem, and a client that reported "Bad Request" instead would be
  // throwing away the only useful thing it was told.
  EXPECT_TRUE(got.body.contains("Layer FOO is not published here"))
    << "the exception report was discarded because the status was not 200";
}

TEST(HttpClientTest, SomethingThatNamesNoAddressIsRefusedBeforeAnythingIsSent)
{
  HttpClient client;

  EXPECT_EQ(client.get(QUrl(), [](const HttpResponse &) {}), 0u);
  EXPECT_EQ(client.get(QUrl(QStringLiteral("not a url")),
                       [](const HttpResponse &) {}),
            0u);
  EXPECT_EQ(client.inFlightCount(), 0);
}

TEST(HttpClientTest, AnAnswerAlreadyOnDiskIsNotAskedForAgain)
{
  // Written where it can be opened and looked at afterwards rather than
  // into a temporary directory.
  const QString cacheDirectory =
    QStringLiteral(HYDROCOUPLEOGC_TEST_OUTPUT_DIR "/httpcache");

  QDir(cacheDirectory).removeRecursively();

  LocalServer server;
  server.reply(200, "image/png", "tile-bytes", "Cache-Control: max-age=600\r\n");

  HttpClient client;
  client.setCacheDirectory(cacheDirectory);

  const QUrl url = server.urlFor(QStringLiteral("/tile/3/4/5.png"));

  HttpResponse first;
  bool firstDone = false;

  client.get(url, [&](const HttpResponse &r) {
    first = r;
    firstDone = true;
  });

  ASSERT_TRUE(waitFor([&] { return firstDone; }));
  ASSERT_TRUE(first.ok) << first.error.toStdString();
  ASSERT_EQ(server.requestCount(), 1);

  // The same tile, asked for again after the first ask has finished — so
  // this is the cache answering and not two callers sharing one request.
  HttpResponse second;
  bool secondDone = false;

  client.get(url, [&](const HttpResponse &r) {
    second = r;
    secondDone = true;
  });

  ASSERT_TRUE(waitFor([&] { return secondDone; }));

  EXPECT_TRUE(second.ok) << second.error.toStdString();
  EXPECT_EQ(second.body, QByteArray("tile-bytes"));
  EXPECT_EQ(server.requestCount(), 1)
    << "a tile the server said to keep for ten minutes was fetched twice";
}

// ── who is asking ───────────────────────────────────────────────────────────

TEST(HttpClientTest, CredentialsAndHeadersReachTheServerAsHeaders)
{
  LocalServer server;

  ServiceCredentials credentials;
  credentials.username = QStringLiteral("hydro");
  credentials.password = QStringLiteral("couple");
  credentials.headers.insert(QStringLiteral("referer"),
                             QStringLiteral("https://hydrocouple.org/"));
  credentials.headers.insert(QStringLiteral("X-Api-Key"),
                             QStringLiteral("abc123"));

  HttpClient client;
  bool called = false;

  client.get(server.urlFor(QStringLiteral("/wms")), credentials,
             [&](const HttpResponse &) { called = true; });

  ASSERT_TRUE(waitFor([&] { return called; }));
  ASSERT_EQ(server.requestCount(), 1);

  const RecordedRequest &request = server.requests().first();

  // Basic authentication is sent up front rather than waiting to be
  // challenged, which would cost a round trip on every tile.
  EXPECT_EQ(request.headers.value("authorization"),
            QByteArray("Basic ") + QByteArray("hydro:couple").toBase64());

  // "referer" is spelled the way HTTP misspells it, whatever the settings
  // key says, because that is the header a server gating on it checks.
  EXPECT_EQ(request.headers.value("referer"),
            QByteArray("https://hydrocouple.org/"));
  EXPECT_EQ(request.headers.value("x-api-key"), QByteArray("abc123"));
  EXPECT_FALSE(request.headers.value("user-agent").isEmpty());
}

TEST(HttpClientTest, ASecretInAUrlIsNotWrittenDownWithTheRequest)
{
  const QUrl url(QStringLiteral(
    "https://user:pass@tiles.example.org/wmts?LAYER=aerial&api_key=SEKRET"
    "&access_token=ALSOSEKRET&FORMAT=image/png"));

  const QString redacted = redactedUrl(url);

  // This library logs what it asks for, and half of these services are
  // addressed with the key in the query string.
  EXPECT_FALSE(redacted.contains(QStringLiteral("SEKRET")))
    << redacted.toStdString();
  EXPECT_FALSE(redacted.contains(QStringLiteral("pass")))
    << redacted.toStdString();

  // And it is still a legible log line.
  EXPECT_TRUE(redacted.contains(QStringLiteral("tiles.example.org")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("LAYER=aerial")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("FORMAT=image/png")));
}

// ── one ask, however many askers ────────────────────────────────────────────

TEST(HttpClientTest, TheSameAskIsMadeOnceHoweverManyCallersWantIt)
{
  LocalServer server;
  server.reply(200, "image/png", "tile-bytes");
  server.hold();

  HttpClient client;
  int answered = 0;

  const QUrl url = server.urlFor(QStringLiteral("/tile/10/357/558.png"));

  client.get(url, [&](const HttpResponse &r) { answered += r.ok ? 1 : 0; });
  client.get(url, [&](const HttpResponse &r) { answered += r.ok ? 1 : 0; });
  client.get(url, [&](const HttpResponse &r) { answered += r.ok ? 1 : 0; });

  ASSERT_TRUE(waitFor([&] { return server.requestCount() >= 1; }));

  // Panning a map asks for the same tile from several places at once.
  EXPECT_EQ(client.pendingCount(), 3);
  EXPECT_EQ(client.inFlightCount(), 1)
    << "the same tile was fetched more than once";

  server.release();

  ASSERT_TRUE(waitFor([&] { return answered == 3; }));
  EXPECT_EQ(server.requestCount(), 1);
  EXPECT_EQ(client.pendingCount(), 0);
}

TEST(HttpClientTest, TwoCallersWhoDisagreeAboutCredentialsDoNotShareAnAnswer)
{
  LocalServer server;
  server.hold();

  ServiceCredentials credentials;
  credentials.username = QStringLiteral("hydro");
  credentials.password = QStringLiteral("couple");

  HttpClient client;
  const QUrl url = server.urlFor(QStringLiteral("/private"));

  client.get(url, credentials, [](const HttpResponse &) {});
  client.get(url, [](const HttpResponse &) {});

  ASSERT_TRUE(waitFor([&] { return server.requestCount() >= 2; }));

  // Sharing these would hand the caller who has no account the answer that
  // the caller with one paid for.
  EXPECT_EQ(client.inFlightCount(), 2);

  server.release();
  waitFor([&] { return client.pendingCount() == 0; });
}

// ── giving up ───────────────────────────────────────────────────────────────

TEST(HttpClientTest, ACallerThatGivesUpIsNotAnswered)
{
  LocalServer server;
  server.hold();

  HttpClient client;
  bool called = false;

  const auto id = client.get(server.urlFor(QStringLiteral("/tile.png")),
                             [&](const HttpResponse &) { called = true; });

  ASSERT_TRUE(waitFor([&] { return server.requestCount() >= 1; }));

  client.cancel(id);

  EXPECT_EQ(client.pendingCount(), 0);
  EXPECT_EQ(client.inFlightCount(), 0)
    << "nobody wanted it any more and it was still being fetched";

  server.release();
  waitFor([&] { return called; }, 300);

  // A zoom change abandons a screenful of tiles at once, and every one of
  // those callbacks would be reaching into a layer that has moved on.
  EXPECT_FALSE(called) << "a caller that had withdrawn was called anyway";
}

TEST(HttpClientTest, OneCallerGivingUpDoesNotStrandTheOthers)
{
  LocalServer server;
  server.reply(200, "image/png", "tile-bytes");
  server.hold();

  HttpClient client;
  bool firstCalled = false;
  bool secondCalled = false;

  const QUrl url = server.urlFor(QStringLiteral("/tile.png"));

  const auto first = client.get(
    url, [&](const HttpResponse &) { firstCalled = true; });
  client.get(url, [&](const HttpResponse &) { secondCalled = true; });

  ASSERT_TRUE(waitFor([&] { return server.requestCount() >= 1; }));
  ASSERT_EQ(client.inFlightCount(), 1);

  client.cancel(first);

  // The request is shared, so withdrawing from it must not abandon it.
  EXPECT_EQ(client.inFlightCount(), 1)
    << "one caller withdrawing killed a request another was waiting on";

  server.release();

  ASSERT_TRUE(waitFor([&] { return secondCalled; }));
  EXPECT_FALSE(firstCalled);
}

TEST(HttpClientTest, GivingUpOnEverythingAnswersNobody)
{
  LocalServer server;
  server.hold();

  HttpClient client;
  int answered = 0;

  client.get(server.urlFor(QStringLiteral("/a.png")),
             [&](const HttpResponse &) { ++answered; });
  client.get(server.urlFor(QStringLiteral("/b.png")),
             [&](const HttpResponse &) { ++answered; });
  client.get(server.urlFor(QStringLiteral("/b.png")),
             [&](const HttpResponse &) { ++answered; });

  ASSERT_TRUE(waitFor([&] { return server.requestCount() >= 2; }));
  ASSERT_EQ(client.pendingCount(), 3);

  // What a map does when the project is closed under it.
  client.cancelAll();

  EXPECT_EQ(client.pendingCount(), 0);
  EXPECT_EQ(client.inFlightCount(), 0);

  server.release();
  waitFor([&] { return answered > 0; }, 300);

  EXPECT_EQ(answered, 0);
}

TEST(HttpClientTest, EverythingIsGivenUpWhenTheClientGoesAway)
{
  LocalServer server;
  server.hold();

  bool called = false;

  {
    HttpClient client;

    client.get(server.urlFor(QStringLiteral("/tile.png")),
               [&](const HttpResponse &) { called = true; });

    ASSERT_TRUE(waitFor([&] { return server.requestCount() >= 1; }));
  }

  // The layer that asked has been closed; its callback captured it.
  server.release();
  waitFor([&] { return called; }, 300);

  EXPECT_FALSE(called);
}

int main(int argc, char **argv)
{
  // A network access manager needs an event loop to run on.
  QCoreApplication application(argc, argv);

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
