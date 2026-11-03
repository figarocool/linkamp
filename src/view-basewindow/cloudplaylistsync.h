#ifndef CLOUDPLAYLISTSYNC_H
#define CLOUDPLAYLISTSYNC_H

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>
#include <functional>

class QNetworkReply;

class CloudPlaylistSync final : public QObject
{
    Q_OBJECT
public:
    enum class Provider { GoogleDrive, OneDrive };
    Q_ENUM(Provider)

    explicit CloudPlaylistSync(QObject *parent = nullptr);
    bool isConnected() const { return m_connected; }
    QString providerName() const;
    void connectProvider(Provider provider);
    void resumeSavedProvider();
    void disconnectProvider();
    void publish(const QByteArray &playlist);

signals:
    void playlistDownloaded(const QByteArray &data);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

private:
    void beginAuthorization();
    void handleBrowserRequest();
    void exchangeCode(const QString &code);
    void refreshAccessToken(std::function<void(bool)> done = {});
    void checkRemote();
    void downloadRemote(const QString &version);
    void uploadRemote();
    void apiRequest(const QByteArray &method, const QUrl &url,
                    const QByteArray &body,
                    std::function<void(int, QByteArray)> done,
                    const QByteArray &contentType = {},
                    bool retryAfterAuthRefresh = true);
    QString clientId() const;
    QString clientSecret() const;
    QString refreshTokenKey() const;
    QString remoteFileKey() const;
    QString remoteVersionKey() const;
    void setProvider(Provider provider);
    void saveProvider(Provider provider);

    QNetworkAccessManager m_network;
    QTcpServer m_callbackServer;
    QTimer m_pollTimer;
    Provider m_provider = Provider::GoogleDrive;
    QByteArray m_playlistData;
    QByteArray m_accessToken;
    QString m_refreshToken;
    QString m_codeVerifier;
    QString m_state;
    QString m_redirectUri;
    QString m_remoteFileId;
    QString m_remoteVersion;
    bool m_connected = false;
    bool m_uploading = false;
    bool m_polling = false;
    bool m_localDirty = false;
};

#endif // CLOUDPLAYLISTSYNC_H
