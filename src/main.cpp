// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
// Copyright (C) 2023 Rodrigo Mendez.
// Copyright (C) 2026 Stefano Basile <stefanobasile@usa.com>.

#include "mainwindow.h"
#include "scale.h"
#include "updatemanager.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QSettings>
#include <QIcon>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "util.h"

#ifndef LINKAMP_VERSION
#define LINKAMP_VERSION "0.0.0"
#endif

static QString appServerName()
{
    QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDirectory.isEmpty())
        runtimeDirectory = QDir::tempPath();
    return QDir(runtimeDirectory).filePath(
        QStringLiteral("linkamp-youtube-%1.sock")
            .arg(qEnvironmentVariable("USER", QStringLiteral("user"))));
}

#ifdef Q_OS_WIN
static void registerLinkampUrlProtocol()
{
    wchar_t executablePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return;

    const QString command = QStringLiteral("\"%1\" \"%2\"")
        .arg(QString::fromWCharArray(executablePath), QStringLiteral("%1"));
    HKEY protocolKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\linkamp", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &protocolKey, nullptr) != ERROR_SUCCESS)
        return;

    const wchar_t protocolDescription[] = L"URL:Linkamp Protocol";
    RegSetValueExW(protocolKey, nullptr, 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(protocolDescription), sizeof(protocolDescription));
    const wchar_t urlProtocol[] = L"";
    RegSetValueExW(protocolKey, L"URL Protocol", 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(urlProtocol), sizeof(urlProtocol));
    RegCloseKey(protocolKey);

    HKEY commandKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Classes\\linkamp\\shell\\open\\command", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &commandKey, nullptr) != ERROR_SUCCESS)
        return;
    const std::wstring commandWide = command.toStdWString();
    RegSetValueExW(commandKey, nullptr, 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(commandWide.c_str()),
                   static_cast<DWORD>((commandWide.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(commandKey);
}
#endif

static QString youtubeUrlArgument(const QString &argument)
{
    QUrl url(argument);
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("linkamp") || scheme == QLatin1String("linamp")) {
        const QUrlQuery query(url);
        url = QUrl::fromUserInput(query.queryItemValue(QLatin1String("url"), QUrl::FullyDecoded));
    } else {
        url = QUrl::fromUserInput(argument);
    }
    return isYouTubeUrl(url) ? url.toString(QUrl::FullyEncoded) : QString();
}

static QJsonObject skinInstallArgument(const QString &argument)
{
    const QUrl requestUrl(argument);
    if (requestUrl.scheme().compare(QStringLiteral("linkamp"), Qt::CaseInsensitive) != 0
        || requestUrl.host().compare(QStringLiteral("install-skin"), Qt::CaseInsensitive) != 0)
        return {};

    const QUrlQuery query(requestUrl);
    const QUrl skinUrl(query.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded));
    const QString fileName = query.queryItemValue(QStringLiteral("name"), QUrl::FullyDecoded);
    if (skinUrl.isEmpty() || fileName.isEmpty())
        return {};
    return {{QStringLiteral("url"), skinUrl.toString(QUrl::FullyEncoded)},
            {QStringLiteral("name"), fileName}};
}

static bool sendUrlsToRunningInstance(const QString &serverName, const QStringList &urls,
                                     const QJsonArray &skins, int timeoutMs)
{
    QLocalSocket existingInstance;
    existingInstance.connectToServer(serverName);
    if (!existingInstance.waitForConnected(timeoutMs))
        return false;

    QJsonArray jsonUrls;
    for (const QString &url : urls)
        jsonUrls.append(url);
    const QByteArray message = QJsonDocument(QJsonObject{
        {QStringLiteral("urls"), jsonUrls}, {QStringLiteral("skins"), skins}})
                                   .toJson(QJsonDocument::Compact) + '\n';
    const bool sent = existingInstance.write(message) == message.size()
                      && existingInstance.waitForBytesWritten(1000);
    existingInstance.disconnectFromServer();
    return sent;
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    registerLinkampUrlProtocol();
#endif
    QApplication app(argc, argv);
    const QIcon linkampIcon(QStringLiteral(":/assets/linkamp-mark.png"));
    app.setWindowIcon(linkampIcon);

    QCoreApplication::setApplicationName("Linkamp");
    QCoreApplication::setOrganizationName("Linkamp");
    QCoreApplication::setApplicationVersion(QStringLiteral(LINKAMP_VERSION));
    QSettings oldSettings(QStringLiteral("Rod"), QStringLiteral("Linamp"));
    QSettings linkampSettings;
    if (linkampSettings.allKeys().isEmpty() && !oldSettings.allKeys().isEmpty()) {
        for (const QString &key : oldSettings.allKeys())
            linkampSettings.setValue(key, oldSettings.value(key));
        linkampSettings.sync();
    }
    QCommandLineParser parser;
    parser.setApplicationDescription("Linkamp");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("url", "The URL(s) to open.");
    parser.process(app);

    QStringList requestedUrls;
    QJsonArray requestedSkins;
    for (const QString &argument : parser.positionalArguments()) {
        const QString url = youtubeUrlArgument(argument);
        if (!url.isEmpty())
            requestedUrls.append(url);
        const QJsonObject skin = skinInstallArgument(argument);
        if (!skin.isEmpty())
            requestedSkins.append(skin);
    }

    const QString serverName = appServerName();
    if (sendUrlsToRunningInstance(serverName, requestedUrls, requestedSkins, 350))
        return 0;

    QLockFile instanceLock(serverName + QStringLiteral(".lock"));
    instanceLock.setStaleLockTime(1000);
    if (!instanceLock.tryLock(2000)) {
        if (sendUrlsToRunningInstance(serverName, requestedUrls, requestedSkins, 1500))
            return 0;
        qWarning() << "Another Linkamp instance is starting; request could not be forwarded";
        return 1;
    }

    // Only remove an orphaned socket after taking the per-user startup lock.
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(serverName)) {
        qWarning() << "Linkamp URL bridge could not listen:" << server.errorString();
        return 1;
    }
    qInfo() << "Linkamp URL bridge listening at" << server.fullServerName();

    MainWindow window;
    auto *updateManager = new UpdateManager(&app);
    QTimer::singleShot(45000, updateManager, &UpdateManager::checkForUpdates);
    QHash<QLocalSocket *, QByteArray> pendingMessages;
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&]() {
        while (server.hasPendingConnections()) {
            QLocalSocket *client = server.nextPendingConnection();
            pendingMessages.insert(client, QByteArray());
            QObject::connect(client, &QLocalSocket::readyRead, &app, [&, client]() {
                QByteArray &buffer = pendingMessages[client];
                buffer.append(client->readAll());
                qsizetype newline = -1;
                while ((newline = buffer.indexOf('\n')) >= 0) {
                    const QByteArray line = buffer.left(newline);
                    buffer.remove(0, newline + 1);
                    const QJsonDocument message = QJsonDocument::fromJson(line);
                    if (!message.isObject())
                        continue;

                    const QJsonArray urls = message.object().value(QStringLiteral("urls")).toArray();
                    const QJsonArray skins = message.object().value(QStringLiteral("skins")).toArray();
                    window.show();
                    window.raise();
                    window.activateWindow();
                    for (const QJsonValue &value : urls)
                        window.addYouTubeUrl(value.toString());
                    for (const QJsonValue &value : skins) {
                        const QJsonObject skin = value.toObject();
                        window.installSkinFromUrl(QUrl(skin.value(QStringLiteral("url")).toString()),
                                                  skin.value(QStringLiteral("name")).toString());
                    }
                }
            });
            QObject::connect(client, &QLocalSocket::disconnected, &app, [&, client]() {
                pendingMessages.remove(client);
                client->deleteLater();
            });
        }
    });

    #ifdef IS_EMBEDDED
    window.setWindowState(Qt::WindowFullScreen);
    #endif
    window.show();
    for (const QString &url : requestedUrls)
        QTimer::singleShot(0, &window, [&window, url]() { window.addYouTubeUrl(url); });
    for (const QJsonValue &value : requestedSkins) {
        const QJsonObject skin = value.toObject();
        const QUrl url(skin.value(QStringLiteral("url")).toString());
        const QString name = skin.value(QStringLiteral("name")).toString();
        QTimer::singleShot(0, &window, [&window, url, name] {
            window.installSkinFromUrl(url, name);
        });
    }

    return app.exec();
}
