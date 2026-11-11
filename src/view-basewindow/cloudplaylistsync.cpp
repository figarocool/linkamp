#include "cloudplaylistsync.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QSettings>
#include <QTcpSocket>
#include <QUrlQuery>

namespace {
QString randomUrlToken(int bytes)
{
    QByteArray value(bytes, Qt::Uninitialized);
    for (char &byte : value)
        byte = static_cast<char>(QRandomGenerator::system()->generate() & 0xff);
    return QString::fromLatin1(value.toBase64(QByteArray::Base64UrlEncoding
                                               | QByteArray::OmitTrailingEquals));
}

QByteArray formEncode(const QUrlQuery &query)
{
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

bool hasPlaylistEntries(const QByteArray &data)
{
    const QList<QByteArray> lines = data.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (!line.isEmpty() && !line.startsWith('#'))
            return true;
    }
    return false;
}
}

CloudPlaylistSync::CloudPlaylistSync(QObject *parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(15000);
    connect(&m_pollTimer, &QTimer::timeout, this, &CloudPlaylistSync::checkRemote);
    connect(&m_callbackServer, &QTcpServer::newConnection,
            this, &CloudPlaylistSync::handleBrowserRequest);
}

QString CloudPlaylistSync::providerName() const
{
    return m_provider == Provider::GoogleDrive ? tr("Google Drive") : tr("OneDrive");
}

QString CloudPlaylistSync::clientId() const
{
    const QString key = m_provider == Provider::GoogleDrive
        ? QStringLiteral("cloud/google/clientId")
        : QStringLiteral("cloud/onedrive/clientId");
    QString configured = QSettings().value(key).toString().trimmed();
    if (!configured.isEmpty())
        return configured;
    if (m_provider == Provider::GoogleDrive) {
        configured = qEnvironmentVariable("LINKAMP_GOOGLE_OAUTH_CLIENT_ID");
#ifdef LINKAMP_GOOGLE_OAUTH_CLIENT_ID
        if (configured.isEmpty())
            configured = QStringLiteral(LINKAMP_GOOGLE_OAUTH_CLIENT_ID);
#endif
    } else {
        configured = qEnvironmentVariable("LINKAMP_ONEDRIVE_OAUTH_CLIENT_ID");
#ifdef LINKAMP_ONEDRIVE_OAUTH_CLIENT_ID
        if (configured.isEmpty())
            configured = QStringLiteral(LINKAMP_ONEDRIVE_OAUTH_CLIENT_ID);
#endif
    }
    return configured.trimmed();
}

QString CloudPlaylistSync::clientSecret() const
{
    QString configured = QSettings().value(QStringLiteral("cloud/google/clientSecret"))
                             .toString().trimmed();
    if (configured.isEmpty())
        configured = qEnvironmentVariable("LINKAMP_GOOGLE_OAUTH_CLIENT_SECRET");
    return configured.trimmed();
}

QString CloudPlaylistSync::refreshTokenKey() const
{
    return m_provider == Provider::GoogleDrive
        ? QStringLiteral("cloud/google/refreshToken")
        : QStringLiteral("cloud/onedrive/refreshToken");
}

QString CloudPlaylistSync::remoteFileKey() const
{
    return m_provider == Provider::GoogleDrive
        ? QStringLiteral("cloud/google/fileId")
        : QStringLiteral("cloud/onedrive/fileId");
}

QString CloudPlaylistSync::remoteVersionKey() const
{
    return m_provider == Provider::GoogleDrive
        ? QStringLiteral("cloud/google/version")
        : QStringLiteral("cloud/onedrive/version");
}

void CloudPlaylistSync::setProvider(Provider provider)
{
    m_provider = provider;
    m_refreshToken = QSettings().value(refreshTokenKey()).toString();
    m_remoteFileId = QSettings().value(remoteFileKey()).toString();
    m_remoteVersion = QSettings().value(remoteVersionKey()).toString();
}

void CloudPlaylistSync::saveProvider(Provider provider)
{
    QSettings().setValue(QStringLiteral("cloud/provider"),
                         provider == Provider::GoogleDrive
                             ? QStringLiteral("google") : QStringLiteral("onedrive"));
}

void CloudPlaylistSync::connectProvider(Provider provider)
{
    setProvider(provider);
    saveProvider(provider);
    m_connected = false;
    m_pollTimer.stop();
    m_refreshToken.clear();
    QSettings().remove(refreshTokenKey());
    m_remoteFileId.clear();
    m_remoteVersion.clear();
    m_localDirty = false;
    QSettings().remove(remoteFileKey());
    QSettings().remove(remoteVersionKey());

    if (clientId().isEmpty()) {
        emit errorOccurred(tr("Manca l'ID OAuth dell'app Linkamp per %1."
                              " Va configurato una volta nella build dell'app.").arg(providerName()));
        return;
    }
    beginAuthorization();
}

void CloudPlaylistSync::resumeSavedProvider(bool preferRemote)
{
    const QString provider = QSettings().value(QStringLiteral("cloud/provider")).toString();
    if (provider != QLatin1String("google") && provider != QLatin1String("onedrive"))
        return;
    setProvider(provider == QLatin1String("google") ? Provider::GoogleDrive
                                                     : Provider::OneDrive);
    if (preferRemote)
        m_remoteVersion.clear();
    if (clientId().isEmpty() || m_refreshToken.isEmpty())
        return;
    emit statusChanged(tr("Connessione a %1…").arg(providerName()));
    refreshAccessToken([this](bool ok) {
        if (!ok)
            return;
        m_connected = true;
        checkRemote();
        m_pollTimer.start();
    });
}

void CloudPlaylistSync::disconnectProvider()
{
    m_pollTimer.stop();
    m_connected = false;
    m_accessToken.clear();
    m_refreshToken.clear();
    m_remoteFileId.clear();
    m_remoteVersion.clear();
    m_localDirty = false;
    QSettings().remove(QStringLiteral("cloud/provider"));
    QSettings().remove(refreshTokenKey());
    QSettings().remove(remoteFileKey());
    QSettings().remove(remoteVersionKey());
    emit statusChanged(tr("Playlist cloud scollegata"));
}

void CloudPlaylistSync::publish(const QByteArray &playlist)
{
    // A transient empty model during startup or a failed restore must never
    // replace a populated cloud backup with a header-only M3U file.
    if (!hasPlaylistEntries(playlist)) {
        emit statusChanged(tr("Playlist vuota: mantengo la copia cloud esistente."));
        return;
    }
    m_playlistData = playlist;
    m_localDirty = true;
    if (m_connected && !m_uploading && !m_polling)
        uploadRemote();
}

void CloudPlaylistSync::reportPlaylistLoadResult(bool applied, const QString &errorMessage)
{
    if (!m_pendingRemoteVersion.isEmpty()) {
        // Remember an unreadable remote version to avoid showing the same
        // parser error on every poll. A forced restore clears this version.
        m_remoteVersion = m_pendingRemoteVersion;
        QSettings().setValue(remoteVersionKey(), m_remoteVersion);
        m_pendingRemoteVersion.clear();
    }
    if (applied) {
        emit statusChanged(tr("Playlist aggiornata da %1").arg(providerName()));
    } else {
        emit errorOccurred(errorMessage.isEmpty()
            ? tr("Non riesco a caricare la playlist scaricata da %1.").arg(providerName())
            : errorMessage);
    }
}

void CloudPlaylistSync::beginAuthorization()
{
    ++m_authAttempt;
    m_oauthAttemptsByPort.clear();
    // A previous login attempt may have been cancelled in the browser while
    // its local listener was still open. Reset it before binding a new port.
    if (m_callbackServer.isListening())
        m_callbackServer.close();
    if (!m_callbackServer.listen(QHostAddress::LocalHost, 0)) {
        emit errorOccurred(tr("Non riesco ad aprire il callback locale per il login: %1")
                               .arg(m_callbackServer.errorString()));
        return;
    }

    m_codeVerifier = randomUrlToken(48);
    m_state = randomUrlToken(24);
    const QByteArray challenge = QCryptographicHash::hash(
        m_codeVerifier.toLatin1(), QCryptographicHash::Sha256).toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    // Use the IPv4 loopback address explicitly. Several Linux browsers resolve
    // "localhost" to ::1 first, while the listener below is bound to 127.0.0.1.
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1").arg(m_callbackServer.serverPort());
    m_oauthAttemptsByPort.insert(m_callbackServer.serverPort(),
        OAuthAttempt{m_authAttempt, m_state, m_codeVerifier, m_redirectUri});
    const QUrl redirect(m_redirectUri);
    QUrl authorizeUrl(m_provider == Provider::GoogleDrive
        ? QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")
        : QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/authorize"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId());
    query.addQueryItem(QStringLiteral("redirect_uri"), redirect.toString());
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), m_provider == Provider::GoogleDrive
        ? QStringLiteral("https://www.googleapis.com/auth/drive.appdata")
        : QStringLiteral("offline_access User.Read Files.ReadWrite.AppFolder"));
    query.addQueryItem(QStringLiteral("state"), m_state);
    query.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(challenge));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    if (m_provider == Provider::GoogleDrive) {
        query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
        query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    }
    authorizeUrl.setQuery(query);
    emit statusChanged(tr("Completa l'accesso a %1 nel browser…").arg(providerName()));
    // On some Linux desktops Qt's default-browser integration fails even
    // Try an installed browser directly so OAuth can still use the user's
    const QStringList browsers = {
        QStringLiteral("chromium"), QStringLiteral("chromium-browser"),
        QStringLiteral("google-chrome"), QStringLiteral("firefox")
    };
#ifdef Q_OS_LINUX
    // Prefer Chromium on Linux: QDesktopServices may report success after
    // handing the URL to a stale desktop-browser setting (e.g. a removed
    // Firefox snap), even though no browser opens.
    for (const QString &browser : browsers) {
        const QString executable = QStandardPaths::findExecutable(browser);
        if (!executable.isEmpty()
            && QProcess::startDetached(executable, {authorizeUrl.toString()}))
            return;
    }
#endif

    if (QDesktopServices::openUrl(authorizeUrl))
        return;

#ifndef Q_OS_LINUX
    for (const QString &browser : browsers) {
        const QString executable = QStandardPaths::findExecutable(browser);
        if (!executable.isEmpty()
            && QProcess::startDetached(executable, {authorizeUrl.toString()}))
            return;
    }
#endif

    m_callbackServer.close();
    emit errorOccurred(tr("Non riesco ad aprire il browser predefinito né un browser installato."));
}

void CloudPlaylistSync::handleBrowserRequest()
{
    QTcpSocket *socket = m_callbackServer.nextPendingConnection();
    if (!socket)
        return;
    const quint16 localPort = socket->localPort();
    const bool attemptKnown = m_oauthAttemptsByPort.contains(localPort);
    const OAuthAttempt attempt = m_oauthAttemptsByPort.value(localPort);
    connect(socket, &QTcpSocket::readyRead, this,
            [this, socket, localPort, attemptKnown, attempt] {
        if (socket->property("oauthRequestHandled").toBool())
            return;
        QByteArray request = socket->property("oauthRequest").toByteArray();
        request += socket->readAll();
        socket->setProperty("oauthRequest", request);
        if (!request.contains("\r\n\r\n"))
            return;
        socket->setProperty("oauthRequestHandled", true);
        const QByteArray requestLine = request.left(request.indexOf("\r\n"));
        const QList<QByteArray> parts = requestLine.split(' ');
        QUrl callbackUrl;
        if (parts.size() >= 2)
            callbackUrl = QUrl(QStringLiteral("http://localhost")
                               + QString::fromUtf8(parts.at(1)));
        const QUrlQuery query(callbackUrl);
        const QString state = query.queryItemValue(QStringLiteral("state"));
        const QString code = query.queryItemValue(QStringLiteral("code"));
        const QString error = query.queryItemValue(QStringLiteral("error"));
        const bool staleAttempt = !attemptKnown || attempt.generation != m_authAttempt;
        const bool hasOAuthResponse = !state.isEmpty() || !code.isEmpty() || !error.isEmpty();
        if (staleAttempt || !hasOAuthResponse) {
            const QByteArray body = QByteArrayLiteral("Richiesta ricevuta. Puoi tornare a Linkamp.");
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                          + QByteArray::number(body.size()) + "\r\n\r\n" + body);
            socket->flush();
            socket->disconnectFromHost();
            if (staleAttempt)
                m_oauthAttemptsByPort.remove(localPort);
            return;
        }
        const bool validCallback = !staleAttempt && error.isEmpty()
            && state == attempt.state && !code.isEmpty();
        const QByteArray body = validCallback
            ? QByteArrayLiteral("Accesso completato. Puoi chiudere questa pagina e tornare a Linkamp.")
            : QByteArrayLiteral("Accesso non completato. Torna a Linkamp e riprova.");
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                      + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->flush();
        socket->disconnectFromHost();
        m_oauthAttemptsByPort.remove(localPort);
        if (!validCallback) {
            m_callbackServer.close();
            emit errorOccurred(error.isEmpty() ? tr("Risposta OAuth non valida.")
                                               : tr("Accesso annullato: %1").arg(error));
            return;
        }
        // Invalidate any duplicate callback connections that were accepted
        // before the listener closed; the first valid response owns the flow.
        ++m_authAttempt;
        m_callbackServer.close();
        exchangeCode(code, attempt.codeVerifier, attempt.redirectUri);
    });
}

void CloudPlaylistSync::exchangeCode(const QString &code, const QString &codeVerifier,
                                     const QString &redirectUri)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    const QString secret = m_provider == Provider::GoogleDrive ? clientSecret() : QString();
    if (!secret.isEmpty())
        form.addQueryItem(QStringLiteral("client_secret"), secret);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    form.addQueryItem(QStringLiteral("code_verifier"), codeVerifier);
    const QUrl endpoint(m_provider == Provider::GoogleDrive
        ? QStringLiteral("https://oauth2.googleapis.com/token")
        : QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/token"));
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_network.post(request, formEncode(form));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray response = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString networkError = reply->errorString();
        reply->deleteLater();
        const QJsonObject object = QJsonDocument::fromJson(response).object();
        m_accessToken = object.value(QStringLiteral("access_token")).toString().toLatin1();
        const QString refresh = object.value(QStringLiteral("refresh_token")).toString();
        if (!refresh.isEmpty()) {
            m_refreshToken = refresh;
            QSettings().setValue(refreshTokenKey(), refresh);
        }
        if (status < 200 || status >= 300 || m_accessToken.isEmpty()) {
            QString details = object.value(QStringLiteral("error_description")).toString();
            if (details.isEmpty())
                details = object.value(QStringLiteral("error")).toString();
            if (details.isEmpty())
                details = response.trimmed().isEmpty()
                    ? networkError : QString::fromUtf8(response).trimmed();
            emit errorOccurred(tr("Login %1 fallito (HTTP %2): %3")
                                   .arg(providerName()).arg(status).arg(details));
            return;
        }
        m_connected = true;
        saveProvider(m_provider);
        emit statusChanged(tr("Accesso a %1 completato; sincronizzo la playlist…").arg(providerName()));
        checkRemote();
        m_pollTimer.start();
    });
}

void CloudPlaylistSync::refreshAccessToken(std::function<void(bool)> done)
{
    if (m_refreshToken.isEmpty()) {
        emit errorOccurred(tr("La sessione %1 è scaduta. Ricollega l'account.").arg(providerName()));
        if (done)
            done(false);
        return;
    }
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    const QString secret = m_provider == Provider::GoogleDrive ? clientSecret() : QString();
    if (!secret.isEmpty())
        form.addQueryItem(QStringLiteral("client_secret"), secret);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    if (m_provider == Provider::OneDrive)
        form.addQueryItem(QStringLiteral("scope"), QStringLiteral(
            "offline_access User.Read Files.ReadWrite.AppFolder"));
    const QUrl endpoint(m_provider == Provider::GoogleDrive
        ? QStringLiteral("https://oauth2.googleapis.com/token")
        : QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/token"));
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_network.post(request, formEncode(form));
    connect(reply, &QNetworkReply::finished, this, [this, reply, done = std::move(done)]() mutable {
        const QByteArray response = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString networkError = reply->errorString();
        reply->deleteLater();
        const QJsonObject object = QJsonDocument::fromJson(response).object();
        m_accessToken = object.value(QStringLiteral("access_token")).toString().toLatin1();
        const QString refresh = object.value(QStringLiteral("refresh_token")).toString();
        if (!refresh.isEmpty()) {
            m_refreshToken = refresh;
            QSettings().setValue(refreshTokenKey(), refresh);
        }
        const bool ok = status >= 200 && status < 300 && !m_accessToken.isEmpty();
        if (!ok) {
            QString details = object.value(QStringLiteral("error_description")).toString();
            if (details.isEmpty())
                details = object.value(QStringLiteral("error")).toString();
            if (details.isEmpty())
                details = response.trimmed().isEmpty()
                    ? networkError : QString::fromUtf8(response).trimmed();
            emit errorOccurred(tr("Rinnovo sessione %1 fallito (HTTP %2): %3")
                                   .arg(providerName()).arg(status).arg(details));
        }
        if (done)
            done(ok);
    });
}

void CloudPlaylistSync::apiRequest(const QByteArray &method, const QUrl &url,
                                   const QByteArray &body,
                                   std::function<void(int, QByteArray)> done,
                                   const QByteArray &contentType,
                                   bool retryAfterAuthRefresh)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken);
    if (!contentType.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(contentType));
    QNetworkReply *reply = nullptr;
    if (method == "GET")
        reply = m_network.get(request);
    else if (method == "POST")
        reply = m_network.post(request, body);
    else if (method == "PUT")
        reply = m_network.put(request, body);
    else if (method == "PATCH")
        reply = m_network.sendCustomRequest(request, method, body);
    else
        return;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, method, url, body, contentType,
             retryAfterAuthRefresh, done = std::move(done)]() mutable {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = reply->readAll();
        reply->deleteLater();
        const bool missingIdentity = status == 403
            && responseBody.contains("unregistered callers");
        if (retryAfterAuthRefresh && (status == 401 || missingIdentity)) {
            refreshAccessToken([this, method, url, body, contentType,
                                done = std::move(done), status,
                                responseBody](bool ok) mutable {
                if (ok) {
                    apiRequest(method, url, body, std::move(done), contentType, false);
                } else {
                    done(status, responseBody);
                }
            });
            return;
        }
        done(status, responseBody);
    });
}

void CloudPlaylistSync::checkRemote()
{
    if (!m_connected || m_polling || m_uploading)
        return;
    m_polling = true;
    if (m_provider == Provider::GoogleDrive) {
        if (!m_remoteFileId.isEmpty()) {
            QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                         .arg(QString::fromLatin1(QUrl::toPercentEncoding(m_remoteFileId))));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("fields"), QStringLiteral("id,modifiedTime,md5Checksum"));
            url.setQuery(query);
            apiRequest("GET", url, {}, [this](int status, QByteArray body) {
                m_polling = false;
                if (status == 404) {
                    m_remoteFileId.clear();
                    QSettings().remove(remoteFileKey());
                    uploadRemote();
                    return;
                }
                if (status < 200 || status >= 300) {
                    qWarning().noquote() << "Google Drive metadata request failed:"
                                         << status << QString::fromUtf8(body);
                    emit errorOccurred(tr("Lettura Google Drive fallita (%1).").arg(status));
                    return;
                }
                const QJsonObject object = QJsonDocument::fromJson(body).object();
                QString version = object.value(QStringLiteral("md5Checksum")).toString();
                if (version.isEmpty())
                    version = object.value(QStringLiteral("modifiedTime")).toString();
                if (version.isEmpty() || version == m_remoteVersion) {
                    if (m_localDirty)
                        uploadRemote();
                    return;
                }
                if (m_localDirty) {
                    uploadRemote();
                    return;
                }
                downloadRemote(version);
            });
            return;
        }
        QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("appDataFolder"));
        query.addQueryItem(QStringLiteral("q"), QStringLiteral(
            "name='Linkamp-playlist.m3u8' and 'appDataFolder' in parents and trashed=false"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,md5Checksum,modifiedTime)"));
        url.setQuery(query);
        apiRequest("GET", url, {}, [this](int status, QByteArray body) {
            m_polling = false;
            if (status < 200 || status >= 300) {
                emit errorOccurred(tr("Ricerca playlist su Google Drive fallita (%1).").arg(status));
                return;
            }
            const QJsonArray files = QJsonDocument::fromJson(body).object()
                                         .value(QStringLiteral("files")).toArray();
            if (files.isEmpty()) {
                uploadRemote();
                return;
            }
            const QJsonObject file = files.first().toObject();
            m_remoteFileId = file.value(QStringLiteral("id")).toString();
            QSettings().setValue(remoteFileKey(), m_remoteFileId);
            if (m_localDirty) {
                uploadRemote();
                return;
            }
            QString version = file.value(QStringLiteral("md5Checksum")).toString();
            if (version.isEmpty())
                version = file.value(QStringLiteral("modifiedTime")).toString();
            downloadRemote(version);
        });
        return;
    }

    QUrl url(QStringLiteral("https://graph.microsoft.com/v1.0/me/drive/special/approot:/Linkamp-playlist.m3u8"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("$select"), QStringLiteral("id,eTag"));
    url.setQuery(query);
    apiRequest("GET", url, {}, [this](int status, QByteArray body) {
        m_polling = false;
        if (status == 404) {
            uploadRemote();
            return;
        }
        if (status < 200 || status >= 300) {
            emit errorOccurred(tr("Ricerca playlist su OneDrive fallita (%1).").arg(status));
            return;
        }
        const QJsonObject item = QJsonDocument::fromJson(body).object();
        m_remoteFileId = item.value(QStringLiteral("id")).toString();
        QSettings().setValue(remoteFileKey(), m_remoteFileId);
        const QString version = item.value(QStringLiteral("eTag")).toString();
        if (version.isEmpty() || version == m_remoteVersion) {
            if (m_localDirty)
                uploadRemote();
            return;
        }
        if (m_localDirty) {
            uploadRemote();
            return;
        }
        downloadRemote(version);
    });
}

void CloudPlaylistSync::downloadRemote(const QString &version)
{
    QUrl url;
    if (m_provider == Provider::GoogleDrive) {
        url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(m_remoteFileId))));
    } else {
        url = QUrl(QStringLiteral("https://graph.microsoft.com/v1.0/me/drive/items/%1/content")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(m_remoteFileId))));
    }
    apiRequest("GET", url, {}, [this, version](int status, QByteArray data) {
        if (status < 200 || status >= 300) {
            emit errorOccurred(tr("Download playlist cloud fallito (%1).").arg(status));
            return;
        }
        if (!hasPlaylistEntries(data)) {
            // Do not let an empty playlist from another instance erase tracks
            // already present locally. Keep this cloud version from being
            // downloaded repeatedly; the next non-empty local publish can heal it.
            m_remoteVersion = version;
            QSettings().setValue(remoteVersionKey(), version);
            emit statusChanged(tr("La copia cloud è vuota; mantengo la playlist locale."));
            return;
        }
        m_pendingRemoteVersion = version;
        emit playlistDownloaded(data);
    });
}

void CloudPlaylistSync::uploadRemote()
{
    if (!m_connected || m_uploading)
        return;
    m_uploading = true;
    const QByteArray uploadedData = m_playlistData;
    if (m_provider == Provider::GoogleDrive) {
        const bool create = m_remoteFileId.isEmpty();
        QUrl url(create
            ? QStringLiteral("https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id,md5Checksum,modifiedTime")
            : QStringLiteral("https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media&fields=id,md5Checksum,modifiedTime")
                  .arg(QString::fromLatin1(QUrl::toPercentEncoding(m_remoteFileId))));
        QByteArray body;
        QByteArray type;
        if (create) {
            const QByteArray boundary = "linkamp-playlist-boundary";
            QJsonObject metadata;
            metadata.insert(QStringLiteral("name"), QStringLiteral("Linkamp-playlist.m3u8"));
            metadata.insert(QStringLiteral("mimeType"), QStringLiteral("audio/x-mpegurl"));
            metadata.insert(QStringLiteral("parents"), QJsonArray{QStringLiteral("appDataFolder")});
            body = "--" + boundary + "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n"
                 + QJsonDocument(metadata).toJson(QJsonDocument::Compact)
                 + "\r\n--" + boundary + "\r\nContent-Type: audio/x-mpegurl\r\n\r\n"
                 + uploadedData + "\r\n--" + boundary + "--";
            type = "multipart/related; boundary=" + boundary;
        } else {
            body = uploadedData;
            type = "audio/x-mpegurl";
        }
        apiRequest(create ? "POST" : "PATCH", url, body,
                   [this, uploadedData](int status, QByteArray response) {
            m_uploading = false;
            if (status < 200 || status >= 300) {
                emit errorOccurred(tr("Salvataggio su Google Drive fallito (%1).").arg(status));
                return;
            }
            const QJsonObject item = QJsonDocument::fromJson(response).object();
            const QString id = item.value(QStringLiteral("id")).toString();
            if (!id.isEmpty()) {
                m_remoteFileId = id;
                QSettings().setValue(remoteFileKey(), id);
            }
            m_remoteVersion = item.value(QStringLiteral("md5Checksum")).toString();
            if (m_remoteVersion.isEmpty())
                m_remoteVersion = item.value(QStringLiteral("modifiedTime")).toString();
            QSettings().setValue(remoteVersionKey(), m_remoteVersion);
            m_localDirty = m_playlistData != uploadedData;
            emit statusChanged(tr("Playlist salvata su Google Drive"));
            if (m_localDirty)
                uploadRemote();
        }, type);
        return;
    }

    QUrl url(QStringLiteral("https://graph.microsoft.com/v1.0/me/drive/special/approot:/Linkamp-playlist.m3u8:/content"));
    apiRequest("PUT", url, uploadedData,
               [this, uploadedData](int status, QByteArray response) {
        m_uploading = false;
        if (status < 200 || status >= 300) {
            emit errorOccurred(tr("Salvataggio su OneDrive fallito (%1).").arg(status));
            return;
        }
        const QJsonObject item = QJsonDocument::fromJson(response).object();
        m_remoteFileId = item.value(QStringLiteral("id")).toString();
        m_remoteVersion = item.value(QStringLiteral("eTag")).toString();
        QSettings().setValue(remoteFileKey(), m_remoteFileId);
        QSettings().setValue(remoteVersionKey(), m_remoteVersion);
        m_localDirty = m_playlistData != uploadedData;
        emit statusChanged(tr("Playlist salvata su OneDrive"));
        if (m_localDirty)
            uploadRemote();
    }, "audio/x-mpegurl");
}
