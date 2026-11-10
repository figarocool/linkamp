#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>

class QFile;
class QNetworkReply;
class QJsonObject;

class UpdateManager : public QObject
{
    Q_OBJECT
public:
    explicit UpdateManager(QObject *parent = nullptr);
    void checkForUpdates();

private:
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_releaseReply;
    QPointer<QNetworkReply> m_downloadReply;
    QFile *m_packageFile = nullptr;
    QString m_packagePath;
    QString m_expectedDigest;
    QString m_latestVersion;

    void inspectLatestRelease();
    void downloadDebianUpdate(const QJsonObject &release);
    void installDebianUpdate();
    void installQtInstallerFrameworkUpdate();
    bool launchRestartHelper(qint64 processId) const;
    QString maintenanceToolPath() const;
    bool isInstalledDebPackage() const;
};

#endif // UPDATEMANAGER_H
