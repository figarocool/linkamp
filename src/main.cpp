// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
// Copyright (C) 2023 Rodrigo Mendez.
// Copyright (C) 2026 Stefano Basile <stefanobasile@usa.com>.

#include "mainwindow.h"
#include "scale.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QSettings>

#include "util.h"

#define APP_VERSION_STR "1.6.3"

static QString appServerName()
{
    QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDirectory.isEmpty())
        runtimeDirectory = QDir::tempPath();
    return QDir(runtimeDirectory).filePath(
        QStringLiteral("linkamp-youtube-%1.sock")
            .arg(qEnvironmentVariable("USER", QStringLiteral("user"))));
}

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

static bool sendUrlsToRunningInstance(const QString &serverName, const QStringList &urls,
                                     int timeoutMs)
{
    QLocalSocket existingInstance;
    existingInstance.connectToServer(serverName);
    if (!existingInstance.waitForConnected(timeoutMs))
        return false;

    QJsonArray jsonUrls;
    for (const QString &url : urls)
        jsonUrls.append(url);
    const QByteArray message = QJsonDocument(QJsonObject{{QStringLiteral("urls"), jsonUrls}})
                                   .toJson(QJsonDocument::Compact) + '\n';
    const bool sent = existingInstance.write(message) == message.size()
                      && existingInstance.waitForBytesWritten(1000);
    existingInstance.disconnectFromServer();
    return sent;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("Linkamp");
    QCoreApplication::setOrganizationName("Linkamp");
    QCoreApplication::setApplicationVersion(APP_VERSION_STR);
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
    for (const QString &argument : parser.positionalArguments()) {
        const QString url = youtubeUrlArgument(argument);
        if (!url.isEmpty())
            requestedUrls.append(url);
    }

    const QString serverName = appServerName();
    if (sendUrlsToRunningInstance(serverName, requestedUrls, 350))
        return 0;

    QLockFile instanceLock(serverName + QStringLiteral(".lock"));
    instanceLock.setStaleLockTime(1000);
    if (!instanceLock.tryLock(2000)) {
        if (sendUrlsToRunningInstance(serverName, requestedUrls, 1500))
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
                    window.show();
                    window.raise();
                    window.activateWindow();
                    for (const QJsonValue &value : urls)
                        window.addYouTubeUrl(value.toString());
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

    return app.exec();
}
