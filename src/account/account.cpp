// SPDX-FileCopyrightText: 2021 kaniini <https://git.pleroma.social/kaniini>
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-3.0-only

#include "account/account.h"

#include "account/notificationhandler.h"
#include "network/networkcontroller.h"
#include "tokodon_http_debug.h"

#ifdef HAVE_KUNIFIEDPUSH
#include "ecdh.h"
#include "tokodon_debug.h"
#endif

#include <qt6keychain/keychain.h>

using namespace Qt::Literals::StringLiterals;

Account::Account(const QString &instanceUri, QNetworkAccessManager *nam, bool ignoreSslErrors, bool admin, QObject *parent)
    : AbstractAccount(parent, instanceUri)
    , m_ignoreSslErrors(ignoreSslErrors)
    , m_qnam(nam)
{
    m_preferences = new Preferences(this);
    setInstanceUri(instanceUri);
    m_requestingAdmin = admin;
}

Account::Account(AccountConfig *settings, QNetworkAccessManager *nam, QObject *parent)
    : AbstractAccount(parent)
    , m_qnam(nam)
{
    m_preferences = new Preferences(this);
    m_config = settings;
    connect(this, &Account::authenticated, this, &Account::checkForFollowRequests);
    buildFromSettings();
}

Account::~Account()
{
    m_identityCache.clear();
}

void Account::get(const QUrl &url,
                  bool authenticated,
                  QObject *parent,
                  std::function<void(QNetworkReply *)> reply_cb,
                  std::function<void(QNetworkReply *)> errorCallback)
{
    QNetworkRequest request = makeRequest(url, authenticated);
    qCDebug(TOKODON_HTTP) << "GET" << url;

    QNetworkReply *reply = m_qnam->get(request);
    reply->setParent(parent);
    handleReply(reply, reply_cb, errorCallback);
}

void Account::post(const QUrl &url,
                   const QJsonDocument &doc,
                   bool authenticated,
                   QObject *parent,
                   std::function<void(QNetworkReply *)> reply_cb,
                   std::function<void(QNetworkReply *)> error_cb,
                   QHash<QByteArray, QByteArray> headers)
{
    auto post_data = doc.toJson();

    QNetworkRequest request = makeRequest(url, authenticated);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    for (const auto [headerKey, headerValue] : headers.asKeyValueRange()) {
        request.setRawHeader(headerKey, headerValue);
    }
    qCDebug(TOKODON_HTTP) << "POST" << url << "[" << post_data << "]";

    auto reply = m_qnam->post(request, post_data);
    reply->setParent(parent);
    handleReply(reply, reply_cb, error_cb);
}

void Account::put(const QUrl &url, const QJsonDocument &doc, bool authenticated, QObject *parent, std::function<void(QNetworkReply *)> reply_cb)
{
    auto post_data = doc.toJson();

    QNetworkRequest request = makeRequest(url, authenticated);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    qCDebug(TOKODON_HTTP) << "PUT" << url << "[" << post_data << "]";

    QNetworkReply *reply = m_qnam->put(request, post_data);
    reply->setParent(parent);
    handleReply(reply, reply_cb);
}

void Account::put(const QUrl &url, const QUrlQuery &formdata, bool authenticated, QObject *parent, std::function<void(QNetworkReply *)> reply_cb)
{
    auto post_data = formdata.toString().toLatin1();

    QNetworkRequest request = makeRequest(url, authenticated);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    qCDebug(TOKODON_HTTP) << "PUT" << url << "[" << post_data << "]";

    QNetworkReply *reply = m_qnam->put(request, post_data);
    reply->setParent(parent);
    handleReply(reply, reply_cb);
}

void Account::post(const QUrl &url,
                   const QUrlQuery &formdata,
                   bool authenticated,
                   QObject *parent,
                   std::function<void(QNetworkReply *)> reply_cb,
                   std::function<void(QNetworkReply *)> errorCallback)
{
    auto post_data = formdata.toString().toLatin1();

    QNetworkRequest request = makeRequest(url, authenticated);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    qCDebug(TOKODON_HTTP) << "POST" << url << "[" << post_data << "]";

    QNetworkReply *reply = m_qnam->post(request, post_data);
    reply->setParent(parent);
    handleReply(reply, reply_cb, errorCallback);
}

QNetworkReply *Account::post(const QUrl &url, QHttpMultiPart *message, bool authenticated, QObject *parent, std::function<void(QNetworkReply *)> reply_cb)
{
    QNetworkRequest request = makeRequest(url, authenticated);

    qCDebug(TOKODON_HTTP) << "POST" << url << "(multipart-message)";

    QNetworkReply *reply = m_qnam->post(request, message);
    reply->setParent(parent);
    handleReply(reply, reply_cb);
    return reply;
}

void Account::patch(const QUrl &url, QHttpMultiPart *multiPart, bool authenticated, QObject *parent, std::function<void(QNetworkReply *)> callback)
{
    QNetworkRequest request = makeRequest(url, authenticated);
    qCDebug(TOKODON_HTTP) << "PATCH" << url << "(multipart-message)";

    QNetworkReply *reply = m_qnam->sendCustomRequest(request, "PATCH", multiPart);
    reply->setParent(parent);
    handleReply(reply, callback);
}

void Account::deleteResource(const QUrl &url, bool authenticated, QObject *parent, std::function<void(QNetworkReply *)> callback)
{
    QNetworkRequest request = makeRequest(url, authenticated);

    qCDebug(TOKODON_HTTP) << "DELETE" << url << "(multipart-message)";

    QNetworkReply *reply = m_qnam->deleteResource(request);
    reply->setParent(parent);
    handleReply(reply, callback);
}

QNetworkRequest Account::makeRequest(const QUrl &url, bool authenticated) const
{
    QNetworkRequest request(url);

    if (authenticated && haveToken()) {
        const QByteArray bearer = QStringLiteral("Bearer %1").arg(m_token).toLocal8Bit();
        request.setRawHeader("Authorization", bearer);
    }

    return request;
}

void Account::handleReply(QNetworkReply *reply, std::function<void(QNetworkReply *)> reply_cb, std::function<void(QNetworkReply *)> errorCallback) const
{
    connect(reply, &QNetworkReply::finished, [reply, reply_cb, errorCallback]() {
        reply->deleteLater();
        if (200 != reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) && !reply->url().toString().contains("nodeinfo"_L1)) {
            qCWarning(TOKODON_HTTP) << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) << reply->url();
            if (errorCallback) {
                errorCallback(reply);
            } else {
                Q_EMIT NetworkController::instance().networkErrorOccurred(reply->errorString());
            }
            return;
        }
        if (reply_cb) {
            reply_cb(reply);
        }
    });
    if (m_ignoreSslErrors) {
        connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError> &) {
            reply->ignoreSslErrors();
        });
    }
}

// assumes file is already opened and named
QNetworkReply *Account::upload(const QUrl &filename, std::function<void(QNetworkReply *)> callback)
{
    auto file = new QFile(filename.toLocalFile());
    const QFileInfo info(filename.toLocalFile());
    file->open(QFile::ReadOnly);

    auto mp = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(info.fileName()));
    filePart.setBodyDevice(file);
    file->setParent(mp);

    mp->append(filePart);

    const auto uploadUrl = apiUrl(QStringLiteral("/api/v1/media"));
    qCDebug(TOKODON_HTTP) << "POST" << uploadUrl << "(upload)";

    return post(uploadUrl, mp, true, this, callback);
}

void Account::requestRemoteObject(const QUrl &remoteUrl, QObject *parent, std::function<void(QNetworkReply *)> callback)
{
    auto url = apiUrl(QStringLiteral("/api/v2/search"));
    url.setQuery({
        {QStringLiteral("q"), remoteUrl.toString()},
        {QStringLiteral("resolve"), QStringLiteral("true")},
        {QStringLiteral("limit"), QStringLiteral("1")},
    });
    get(url, true, parent, std::move(callback));
}

static QMap<QString, AbstractAccount::StreamingEventType> stringToStreamingEventType = {
    {QStringLiteral("update"), AbstractAccount::StreamingEventType::UpdateEvent},
    {QStringLiteral("delete"), AbstractAccount::StreamingEventType::DeleteEvent},
    {QStringLiteral("notification"), AbstractAccount::StreamingEventType::NotificationEvent},
    {QStringLiteral("filters_changed"), AbstractAccount::StreamingEventType::FiltersChangedEvent},
    {QStringLiteral("conversation"), AbstractAccount::StreamingEventType::ConversationEvent},
    {QStringLiteral("announcement"), AbstractAccount::StreamingEventType::AnnouncementEvent},
    {QStringLiteral("announcement.reaction"), AbstractAccount::StreamingEventType::AnnouncementRedactedEvent},
    {QStringLiteral("announcement.delete"), AbstractAccount::StreamingEventType::AnnouncementDeletedEvent},
    {QStringLiteral("status.update"), AbstractAccount::StreamingEventType::StatusUpdatedEvent},
    {QStringLiteral("encrypted_message"), AbstractAccount::StreamingEventType::EncryptedMessageChangedEvent},
};

QWebSocket *Account::streamingSocket(const QString &stream)
{
    if (m_token.isEmpty()) {
        return nullptr;
    }

    if (m_websockets.contains(stream)) {
        return m_websockets[stream];
    }

    auto socket = new QWebSocket();
    socket->setParent(this);

    const auto url = streamingUrl(stream);

    connect(socket, &QWebSocket::textMessageReceived, this, [=](const QString &message) {
        const auto env = QJsonDocument::fromJson(message.toLocal8Bit());
        if (env.isObject() && env.object().contains("event"_L1)) {
            const auto event = stringToStreamingEventType[env.object()["event"_L1].toString()];
            Q_EMIT streamingEvent(event, env.object()["payload"_L1].toString().toLocal8Bit());

            if (event == NotificationEvent) {
                const auto doc = QJsonDocument::fromJson(env.object()["payload"_L1].toString().toLocal8Bit());
                handleNotification(doc);
                return;
            }
        }
    });

    socket->open(url);

    m_websockets[stream] = socket;
    return socket;
}

void Account::validateToken(bool newAccount)
{
    const QUrl verify_credentials = apiUrl(QStringLiteral("/api/v1/accounts/verify_credentials"));

    get(
        verify_credentials,
        true,
        this,
        [=](QNetworkReply *reply) {
            if (!reply->isFinished()) {
                return;
            }

            const auto doc = QJsonDocument::fromJson(reply->readAll());

            if (!doc.isObject()) {
                return;
            }

            const auto object = doc.object();
            if (!object.contains("source"_L1)) {
                return;
            }

            m_identity = identityLookup(object["id"_L1].toString(), object);
            m_name = m_identity->username();
            Q_EMIT identityChanged();
            Q_EMIT authenticated(true, {});

#ifdef HAVE_KUNIFIEDPUSH
            if (newAccount) {
                // We asked for the push scope, so we can safely start subscribing to notifications
                config()->setEnablePushNotifications(true);
                config()->save();
            }
#else
            Q_UNUSED(newAccount)
#endif

#ifdef HAVE_KUNIFIEDPUSH
            get(
                apiUrl(QStringLiteral("/api/v1/push/subscription")),
                true,
                this,
                [=](QNetworkReply *reply) {
                    m_hasPushSubscription = true;

                    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());

                    if (!NetworkController::instance().endpoint.isEmpty() && doc["endpoint"_L1] != NetworkController::instance().endpoint) {
                        qWarning(TOKODON_LOG) << "KUnifiedPush endpoint has changed to" << NetworkController::instance().endpoint << ", resubscribing!";

                        deleteResource(apiUrl(QStringLiteral("/api/v1/push/subscription")), true, this, [=](QNetworkReply *reply) {
                            Q_UNUSED(reply)
                            m_hasPushSubscription = false;
                            subscribePushNotifications();
                        });
                    } else {
                        updatePushNotifications();
                    }
                },
                [=](QNetworkReply *reply) {
                    Q_UNUSED(reply);
                    m_hasPushSubscription = false;
                    updatePushNotifications();
                });
#endif
        },
        [=](QNetworkReply *reply) {
            const auto doc = QJsonDocument::fromJson(reply->readAll());

            Q_EMIT authenticated(false, doc.isEmpty() ? reply->errorString() : doc["error"_L1].toString());
        });

    fetchInstanceMetadata();

    // set up streaming for notifications
    streamingSocket(QStringLiteral("user"));
}

void Account::writeToSettings()
{
    // do not write to settings if we do not have complete information yet,
    // or else it writes malformed and possibly duplicate accounts to settings.
    if (m_name.isEmpty() || m_instance_uri.isEmpty()) {
        return;
    }

    AccountConfig config(settingsGroupName());
    config.setClientId(m_client_id);
    config.setInstanceUri(m_instance_uri);
    config.setName(m_name);
    config.setIgnoreSslErrors(m_ignoreSslErrors);

    config.save();

    auto accessTokenJob = new QKeychain::WritePasswordJob{QStringLiteral("Tokodon"), this};
#ifdef SAILFISHOS
    accessTokenJob->setInsecureFallback(true);
#endif
    accessTokenJob->setKey(accessTokenKey());
    accessTokenJob->setTextData(m_token);
    accessTokenJob->start();

    auto clientSecretJob = new QKeychain::WritePasswordJob{QStringLiteral("Tokodon"), this};
#ifdef SAILFISHOS
    clientSecretJob->setInsecureFallback(true);
#endif
    clientSecretJob->setKey(clientSecretKey());
    clientSecretJob->setTextData(m_client_secret);
    clientSecretJob->start();
}

void Account::buildFromSettings()
{
    m_client_id = m_config->clientId();
    m_name = m_config->name();
    m_instance_uri = m_config->instanceUri();
    m_ignoreSslErrors = m_config->ignoreSslErrors();

    auto accessTokenJob = new QKeychain::ReadPasswordJob{QStringLiteral("Tokodon"), this};
#ifdef SAILFISHOS
    accessTokenJob->setInsecureFallback(true);
#endif
    accessTokenJob->setKey(accessTokenKey());

    QObject::connect(accessTokenJob, &QKeychain::ReadPasswordJob::finished, [this, accessTokenJob]() {
        m_token = accessTokenJob->textData();

        validateToken();
    });

    accessTokenJob->start();

    auto clientSecretJob = new QKeychain::ReadPasswordJob{QStringLiteral("Tokodon"), this};
#ifdef SAILFISHOS
    clientSecretJob->setInsecureFallback(true);
#endif
    clientSecretJob->setKey(clientSecretKey());

    QObject::connect(clientSecretJob, &QKeychain::ReadPasswordJob::finished, [this, clientSecretJob]() {
        m_client_secret = clientSecretJob->textData();
    });

    clientSecretJob->start();
}

void Account::checkForFollowRequests()
{
    get(apiUrl(QStringLiteral("/api/v1/follow_requests")), true, this, [this](QNetworkReply *reply) {
        const auto followRequestResult = QJsonDocument::fromJson(reply->readAll());
        if (m_followRequestCount != followRequestResult.array().size()) {
            m_followRequestCount = followRequestResult.array().size();
            Q_EMIT followRequestCountChanged();
        }
    });
}

void Account::updatePushNotifications()
{
#ifdef HAVE_KUNIFIEDPUSH
    auto cfg = config();

    // If push notifications are explicitly disabled (like if we have an account that does not have the scope) skip
    if (!cfg->enablePushNotifications()) {
        return;
    }

    if (m_hasPushSubscription && !cfg->enableNotifications()) {
        unsubscribePushNotifications();
    } else if (!m_hasPushSubscription && cfg->enableNotifications()) {
        subscribePushNotifications();
    } else {
        QUrlQuery formdata = buildNotificationFormData();

        formdata.addQueryItem(QStringLiteral("policy"), QStringLiteral("all"));

        put(apiUrl(QStringLiteral("/api/v1/push/subscription")), formdata, true, this, [=](QNetworkReply *reply) {
            qCDebug(TOKODON_HTTP) << "Updated push notification rules:" << reply->readAll();
        });
    }
#endif
}

void Account::unsubscribePushNotifications()
{
#ifdef HAVE_KUNIFIEDPUSH
    Q_ASSERT(m_hasPushSubscription);
    deleteResource(apiUrl(QStringLiteral("/api/v1/push/subscription")), true, this, [=](QNetworkReply *reply) {
        m_hasPushSubscription = false;
        qCDebug(TOKODON_HTTP) << "Unsubscribed from push notifications:" << reply->readAll();
    });
#endif
}

void Account::subscribePushNotifications()
{
#ifdef HAVE_KUNIFIEDPUSH
    Q_ASSERT(!m_hasPushSubscription);

    // Generate 16 random bytes
    QByteArray randArray;
    for (int i = 0; i < 16; i++) {
        randArray.push_back(QRandomGenerator::global()->generate());
    }

    QUrlQuery formdata = buildNotificationFormData();
    formdata.addQueryItem(QStringLiteral("subscription[endpoint]"), QUrl(NetworkController::instance().endpoint).toString());

    // TODO: save this keypair in the keychain
    const auto keys = generateECDHKeypair();

    formdata.addQueryItem(QStringLiteral("subscription[keys][p256dh]"), QString::fromUtf8(exportPublicKey(keys).toBase64(QByteArray::Base64UrlEncoding)));
    formdata.addQueryItem(QStringLiteral("subscription[keys][auth]"), QString::fromUtf8(randArray.toBase64(QByteArray::Base64UrlEncoding)));

    formdata.addQueryItem(QStringLiteral("data[policy]"), QStringLiteral("all"));

    post(
        apiUrl(QStringLiteral("/api/v1/push/subscription")),
        formdata,
        true,
        this,
        [=](QNetworkReply *reply) {
            m_hasPushSubscription = true;
            qCDebug(TOKODON_HTTP) << "Subscribed to push notifications:" << reply->readAll();
        },
        [=](QNetworkReply *reply) {
            Q_UNUSED(reply); // to prevent a visible error
        });
#endif
}

QUrlQuery Account::buildNotificationFormData()
{
    auto cfg = config();

    QUrlQuery formdata;
    const auto addQuery = [&formdata](const QString key, const bool value) {
        formdata.addQueryItem(QStringLiteral("data[alerts][%1]").arg(key), value ? QStringLiteral("true") : QStringLiteral("false"));
    };

    addQuery(QStringLiteral("mention"), cfg->notifyMention());
    addQuery(QStringLiteral("status"), cfg->notifyStatus());
    addQuery(QStringLiteral("reblog"), cfg->notifyBoost());
    addQuery(QStringLiteral("follow"), cfg->notifyFollow());
    addQuery(QStringLiteral("follow_request"), cfg->notifyFollowRequest());
    addQuery(QStringLiteral("favourite"), cfg->notifyFavorite());
    addQuery(QStringLiteral("poll"), cfg->notifyPoll());
    addQuery(QStringLiteral("update"), cfg->notifyUpdate());
    addQuery(QStringLiteral("admin.sign_up"), cfg->notifySignup());
    addQuery(QStringLiteral("admin.report"), cfg->notifyReport());

    return formdata;
}

void Account::registerTokodon(const bool authCode)
{
    registerApplication(QStringLiteral("Tokodon"),
                        QStringLiteral("https://apps.kde.org/tokodon"),
                        m_requestingAdmin ? QStringLiteral("admin:read admin:write") : QStringLiteral(""),
                        authCode);
}

#include "moc_account.cpp"
