#include "updatemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QVersionNumber>

namespace {
const QUrl kLatestReleaseUrl(QStringLiteral(
    "https://api.github.com/repos/figarocool/linkamp/releases/latest"));
const QUrl kReleasePageUrl(QStringLiteral("https://github.com/figarocool/linkamp/releases/latest"));
constexpr qint64 kUpdateCheckIntervalSeconds = 24 * 60 * 60;

QString debianCodename()
{
    QFile osRelease(QStringLiteral("/etc/os-release"));
    if (!osRelease.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QRegularExpression pattern(QStringLiteral(R"(^VERSION_CODENAME=(.*)$)"));
    while (!osRelease.atEnd()) {
        const auto match = pattern.match(QString::fromUtf8(osRelease.readLine()).trimmed());
        if (match.hasMatch())
            return match.captured(1).remove('"').remove('\'');
    }
    return {};
}

QString debianArchitecture()
{
    const QString architecture = QSysInfo::currentCpuArchitecture().toLower();
    if (architecture == QLatin1String("x86_64") || architecture == QLatin1String("amd64"))
        return QStringLiteral("amd64");
    if (architecture == QLatin1String("aarch64") || architecture == QLatin1String("arm64"))
        return QStringLiteral("arm64");
    return {};
}
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
{
}

void UpdateManager::checkForUpdates()
{
    const QDateTime lastCheck = QSettings().value(QStringLiteral("updates/lastCheck")).toDateTime();
    if (lastCheck.isValid() && lastCheck.secsTo(QDateTime::currentDateTimeUtc())
            < kUpdateCheckIntervalSeconds)
        return;

    QNetworkRequest request(kLatestReleaseUrl);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Linkamp-Updater");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_releaseReply = m_network.get(request);
    connect(m_releaseReply, &QNetworkReply::finished, this, &UpdateManager::inspectLatestRelease);
}

void UpdateManager::inspectLatestRelease()
{
    QNetworkReply *reply = m_releaseReply;
    m_releaseReply = nullptr;
    if (reply == nullptr)
        return;

    const QByteArray response = reply->readAll();
    const bool requestSucceeded = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!requestSucceeded)
        return;
    QSettings().setValue(QStringLiteral("updates/lastCheck"), QDateTime::currentDateTimeUtc());

    const QJsonObject release = QJsonDocument::fromJson(response).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    m_latestVersion = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
    const QVersionNumber latest = QVersionNumber::fromString(m_latestVersion);
    const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
    if (latest.isNull() || current.isNull() || QVersionNumber::compare(latest, current) <= 0)
        return;

#ifdef Q_OS_WIN
    installQtInstallerFrameworkUpdate();
#elif defined(Q_OS_MACOS)
    installQtInstallerFrameworkUpdate();
#elif defined(Q_OS_LINUX)
    if (isInstalledDebPackage())
        downloadDebianUpdate(release);
#endif
}

void UpdateManager::downloadDebianUpdate(const QJsonObject &release)
{
    const QString codename = debianCodename();
    const QString architecture = debianArchitecture();
    if (codename.isEmpty() || architecture.isEmpty())
        return;

    const QRegularExpression assetPattern(QStringLiteral("^linkamp_%1[~.]%2[0-9]+_%3\\.deb$")
        .arg(QRegularExpression::escape(m_latestVersion),
             QRegularExpression::escape(codename),
             QRegularExpression::escape(architecture)));
    QJsonObject matchingAsset;
    for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        if (assetPattern.match(asset.value(QStringLiteral("name")).toString()).hasMatch()) {
            matchingAsset = asset;
            break;
        }
    }

    const QString digest = matchingAsset.value(QStringLiteral("digest")).toString();
    const QString downloadUrl = matchingAsset.value(QStringLiteral("browser_download_url")).toString();
    if (!digest.startsWith(QLatin1String("sha256:")) || downloadUrl.isEmpty())
        return;

    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cachePath.isEmpty() || !QDir().mkpath(cachePath))
        return;

    m_packagePath = QDir(cachePath).filePath(matchingAsset.value(QStringLiteral("name")).toString());
    m_expectedDigest = digest.mid(7).toLower();
    m_packageFile = new QFile(m_packagePath, this);
    if (!m_packageFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_packageFile->deleteLater();
        m_packageFile = nullptr;
        return;
    }

    QNetworkRequest request{QUrl(downloadUrl)};
    request.setRawHeader("User-Agent", "Linkamp-Updater");
    request.setTransferTimeout(120000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_downloadReply = m_network.get(request);
    connect(m_downloadReply, &QIODevice::readyRead, this, [this] {
        if (m_packageFile != nullptr && m_downloadReply != nullptr)
            m_packageFile->write(m_downloadReply->readAll());
    });
    connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateManager::installDebianUpdate);
}

void UpdateManager::installDebianUpdate()
{
    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;
    if (reply == nullptr || m_packageFile == nullptr)
        return;

    if (reply->bytesAvailable() > 0)
        m_packageFile->write(reply->readAll());
    const bool downloadSucceeded = reply->error() == QNetworkReply::NoError
                                   && m_packageFile->flush();
    reply->deleteLater();
    m_packageFile->close();
    const bool hashFileOpened = m_packageFile->open(QIODevice::ReadOnly);
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    const QByteArray actualDigest = hashFileOpened && hasher.addData(m_packageFile)
        ? hasher.result().toHex() : QByteArray();
    m_packageFile->deleteLater();
    m_packageFile = nullptr;

    if (!downloadSucceeded || actualDigest != m_expectedDigest.toLatin1()) {
        QFile::remove(m_packagePath);
        return;
    }

    QTimer::singleShot(0, this, [this] {
        const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        if (pkexec.isEmpty()) {
            QDesktopServices::openUrl(kReleasePageUrl);
            return;
        }
        const bool started = QProcess::startDetached(
            pkexec,
            {QStringLiteral("/usr/bin/apt-get"), QStringLiteral("install"),
             QStringLiteral("--yes"), m_packagePath});
        if (started)
            QCoreApplication::quit();
        else
            QDesktopServices::openUrl(kReleasePageUrl);
    });
}

void UpdateManager::installQtInstallerFrameworkUpdate()
{
    const QString tool = maintenanceToolPath();
    if (tool.isEmpty())
        return;

    const bool started = QProcess::startDetached(
        tool, {QStringLiteral("update"), QStringLiteral("--confirm-command")});
    if (started)
        QCoreApplication::quit();
}

QString UpdateManager::maintenanceToolPath() const
{
    QStringList candidates;
#ifdef Q_OS_WIN
    QString directory = QCoreApplication::applicationDirPath();
    for (int i = 0; i < 4; ++i) {
        candidates.append(QDir(directory).filePath(QStringLiteral("linkamp-maintenance.exe")));
        directory = QDir(directory).filePath(QStringLiteral(".."));
    }
#elif defined(Q_OS_MACOS)
    QString directory = QCoreApplication::applicationDirPath();
    for (int i = 0; i < 5; ++i) {
        candidates.append(QDir(directory).filePath(
            QStringLiteral("linkamp-maintenance.app/Contents/MacOS/linkamp-maintenance")));
        directory = QDir(directory).filePath(QStringLiteral(".."));
    }
#endif
    for (const QString &candidate : candidates) {
        const QFileInfo file(candidate);
        if (file.isFile() && file.isExecutable())
            return file.absoluteFilePath();
    }
    return {};
}

bool UpdateManager::isInstalledDebPackage() const
{
    return QCoreApplication::applicationFilePath() == QLatin1String("/usr/bin/linkamp-player");
}
