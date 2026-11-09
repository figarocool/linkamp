#include <QFileDialog>
#include <QDirIterator>
#include <QCloseEvent>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QRegularExpression>
#include <QHostAddress>

#include "mainwindow.h"
#include "desktopplayerwindow.h"
#include "titlebar.h"
#include "qstandardpaths.h"
#include "ui_desktopplayerwindow.h"
#include "scale.h"
#include "util.h"
#include "winampskin.h"
#include "updatemanager.h"

#ifdef IS_EMBEDDED
#include "embeddedbasewindow.h"
#include "ui_embeddedbasewindow.h"
#else
#include "desktopbasewindow.h"
#include "ui_desktopbasewindow.h"
#endif

#include <QLabel>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLineEdit>
#include <QProgressDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDialog>
#include <QMenu>
#include <QCursor>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QMouseEvent>
#include <QBuffer>

#include <algorithm>

#ifdef IS_EMBEDDED
const unsigned int WINDOW_W = 320 * UI_SCALE;
const unsigned int WINDOW_H = 100 * UI_SCALE;
#else
const unsigned int WINDOW_W = 275 * UI_SCALE;
const unsigned int WINDOW_H = 116 * UI_SCALE;
#endif

#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
#define PY_SSIZE_T_CLEAN
#undef slots
#include <Python.h>
#define slots Q_SLOTS
#define LINKAMP_PY_MODULE "linamp"
#endif

namespace {
int windowBottomOnScreen(const QWidget *window)
{
    return window->frameGeometry().bottom() + 1;
}
}

MainWindow::MainWindow(QWidget *parent)
     : QMainWindow{parent}
{
    // Linux BT and Spotify integrations are provided by the embedded Python bridge.
#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
    Py_Initialize();
    PyEval_SaveThread();
#endif

    // Setup playlist
    m_playlistModel = new PlaylistModel(this);
    m_playlist = m_playlistModel->playlist();
    m_cloudPlaylistSync = new CloudPlaylistSync(this);
    connect(m_cloudPlaylistSync, &CloudPlaylistSync::playlistDownloaded,
            this, &MainWindow::applyCloudPlaylist);
    connect(m_cloudPlaylistSync, &CloudPlaylistSync::statusChanged,
            this, [](const QString &message) { qInfo() << message; });
    connect(m_cloudPlaylistSync, &CloudPlaylistSync::errorOccurred,
            this, [this](const QString &message) {
        QMessageBox::warning(this, tr("Playlist cloud"), message);
    });
    connect(m_playlist, &QMediaPlaylist::mediaInserted, this, &MainWindow::schedulePlaylistSave);
    connect(m_playlist, &QMediaPlaylist::mediaRemoved, this, &MainWindow::schedulePlaylistSave);
    connect(m_playlist, &QMediaPlaylist::mediaChanged, this, &MainWindow::schedulePlaylistSave);
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, [](int index) {
        QSettings().setValue(QStringLiteral("playlist/currentIndex"), index);
    });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSavedPlaylist);

    // Setup views
    controlButtons = new ControlButtonsWidget(this);
    player = new PlayerView(this, controlButtons);
    player->setAttribute(Qt::WidgetAttribute::WA_StyledBackground,  true);

    equalizer = new SystemEqualizer(this);
    player->setEqEnabled(equalizer->isEnabled());
    connect(equalizer, &SystemEqualizer::enabledChanged, player, &PlayerView::setEqEnabled);
    connect(player, &PlayerView::eqClicked, this, &MainWindow::showEqualizer);

    coordinator = new AudioSourceCoordinator(this, player);
    coordinator->setEqualizer(equalizer);
    fileSource = new AudioSourceFile(this, m_playlistModel);
    const auto restoreSelectedTrack = [this] {
        if (!m_playlist || m_playlist->mediaCount() <= 0)
            return;
        const int savedIndex = QSettings().value(QStringLiteral("playlist/currentIndex"), 0).toInt();
        const int index = savedIndex >= 0 && savedIndex < m_playlist->mediaCount()
            ? savedIndex : 0;
        if (m_playlist->currentIndex() != index)
            m_playlist->setCurrentIndex(index);
    };
    connect(m_playlist, &QMediaPlaylist::loaded, this, [this, restoreSelectedTrack] {
        if (m_cloudLoadPending) {
            m_cloudLoadPending = false;
            m_isLoadingPlaylist = false;
            int restoreIndex = m_cloudRestoreIndex;
            if (restoreIndex < 0)
                restoreIndex = QSettings().value(QStringLiteral("playlist/currentIndex"), 0).toInt();
            if (m_playlist->mediaCount() > 0)
                m_playlist->setCurrentIndex(qBound(0, restoreIndex,
                                                   m_playlist->mediaCount() - 1));
            m_cloudRestoreIndex = -1;
        } else {
            restoreSelectedTrack();
        }
        if (playlist)
            playlist->update();
        if (m_cloudResumePending) {
            m_cloudResumePending = false;
            m_cloudPlaylistSync->publish(serializePlaylist());
            m_cloudPlaylistSync->resumeSavedProvider();
        }
    });
    restoreSelectedTrack();
#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
    btSource = new AudioSourcePython(LINKAMP_PY_MODULE, "BTPlayer", this);
    cdSource = new AudioSourceCDNative(this);
    spotSource = new AudioSourcePython(LINKAMP_PY_MODULE, "SpotifyPlayer", this);
#endif
    coordinator->addSource(fileSource, "FILE", true);
#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
    coordinator->addSource(btSource, "BT", false);
    coordinator->addSource(cdSource, "CD", false);
    coordinator->addSource(spotSource, "SPOT", false);
#endif


    // Connect events
    connect(fileSource, &AudioSourceFile::showPlaylistRequested, this, &MainWindow::showPlaylist);
    connect(fileSource, &AudioSourceFile::openFilesRequested, this, &MainWindow::open);

    // Prepare player main view
    #ifdef IS_EMBEDDED
    EmbeddedBaseWindow *playerWindow = new EmbeddedBaseWindow(this);
    #else
    DesktopBaseWindow *playerWindow = new DesktopBaseWindow(this);
    #endif

    playerWindow->setAttribute(Qt::WidgetAttribute::WA_StyledBackground,  true);
    m_playerBodyContainer = playerWindow->ui->bodyContainer;
    if (auto *mainTitleBar = playerWindow->findChild<TitleBar *>(QStringLiteral("titlebarContainer"))) {
        m_mainPlayerWindow = mainTitleBar->window();
        m_mainPlayerWindow->setWindowTitle(QStringLiteral("Linkamp"));
        connect(player, &PlayerView::currentTrackTitleChanged, m_mainPlayerWindow,
                [window = m_mainPlayerWindow](const QString &title) {
            const QString trackTitle = title.trimmed();
            window->setWindowTitle(trackTitle.isEmpty()
                ? QStringLiteral("Linkamp")
                : QStringLiteral("%1 - Linkamp").arg(trackTitle));
        });
        m_mainPlayerWindow->installEventFilter(this);
        // Winamp opens its player menu from a right-click on the main window.
        // The compact Linkamp shell is a separate top-level widget, so handle
        // the context event there as well as from the menu button.
        const QString mainPositionKey = QStringLiteral("windows/main/position");
        if (QSettings().contains(mainPositionKey))
            m_mainPlayerWindow->move(QSettings().value(mainPositionKey).toPoint());
        connect(mainTitleBar, &TitleBar::menuClicked, this, &MainWindow::showMenu);
        connect(mainTitleBar, &TitleBar::shadeClicked, this, &MainWindow::toggleWindowShade);
        connect(mainTitleBar, &TitleBar::moveStarted, this, [this](bool detach) {
            beginAttachedWindowDrag(m_mainPlayerWindow, detach);
        });
        connect(mainTitleBar, &TitleBar::moveFinished, this, [this] {
            finishAttachedWindowDrag(m_mainPlayerWindow);
        });
    }

    DesktopPlayerWindow *playerWindowContent = new DesktopPlayerWindow(this);
    QVBoxLayout *playerLayout = new QVBoxLayout;
    playerLayout->setContentsMargins(0, 0, 0, 0);
    playerLayout->addWidget(player);
    playerWindowContent->ui->playerViewContainer->setLayout(playerLayout);
    QVBoxLayout *buttonsLayout = new QVBoxLayout;
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->addWidget(controlButtons);
    playerWindowContent->ui->controlButtonsContainer->setLayout(buttonsLayout);

    QHBoxLayout *playerContentLayout = new QHBoxLayout;
    playerContentLayout->setContentsMargins(0, 0, 0, 0);
    playerContentLayout->addWidget(playerWindowContent);
    playerWindow->ui->body->setLayout(playerContentLayout);
    if (m_mainPlayerWindow) {
        const auto childWidgets = m_mainPlayerWindow->findChildren<QWidget *>();
        for (QWidget *child : childWidgets)
            child->installEventFilter(this);
    }

    // Keep the classic playlist editor as a separate compact top-level window.
    playlist = new ClassicPlaylistWindow(m_playlist, this);
    if (playlist->property("savedWindowPosition").toBool()) {
        m_lastPlaylistPosition = playlist->pos();
        m_havePlaylistPosition = true;
    }
    playlist->installEventFilter(this);
    connect(player, &PlayerView::playbackPositionChanged, playlist, &ClassicPlaylistWindow::setPosition);
    connect(playlist, &ClassicPlaylistWindow::openFilesRequested, this, &MainWindow::open);
    connect(playlist, &ClassicPlaylistWindow::addFolderRequested, this, &MainWindow::addFolderToPlaylist);
    connect(playlist, &ClassicPlaylistWindow::addYouTubeRequested, this, &MainWindow::promptForYouTubeUrl);
    connect(playlist, &ClassicPlaylistWindow::songActivated, this, [this](int row) {
        coordinator->setSource(0);
        fileSource->jump(m_playlistModel->index(row, 0));
        show();
        raise();
        activateWindow();
    });
    connect(playlist, &ClassicPlaylistWindow::moveStarted, this, [this](bool detach) {
        beginAttachedWindowDrag(playlist, detach);
    });
    connect(playlist, &ClassicPlaylistWindow::moveFinished, this, [this] {
        finishAttachedWindowDrag(playlist);
    });

    milkdrop = new MilkdropWindow(this);
    if (milkdrop->property("savedWindowPosition").toBool()) {
        m_lastMilkdropPosition = milkdrop->pos();
        m_haveMilkdropPosition = true;
    }
    milkdrop->installEventFilter(this);
    connect(player, &PlayerView::audioDataAvailable, milkdrop, &MilkdropWindow::setAudioData);
    connect(player, &PlayerView::playbackStateChanged, milkdrop, &MilkdropWindow::setPlaybackState);
    connect(milkdrop, &MilkdropWindow::moveStarted, this, [this](bool detach) {
        beginAttachedWindowDrag(milkdrop, detach);
    });
    connect(milkdrop, &MilkdropWindow::moveFinished, this, [this] {
        finishAttachedWindowDrag(milkdrop);
    });

    videoWindow = new VideoWindow(this);
    if (videoWindow->property("savedWindowPosition").toBool()) {
        m_lastVideoPosition = videoWindow->pos();
        m_haveVideoPosition = true;
    }
    videoWindow->installEventFilter(this);
    fileSource->setVideoSink(videoWindow->videoSink());
    connect(player, &PlayerView::playbackStateChanged, videoWindow, &VideoWindow::setPlaybackState);
    connect(player, &PlayerView::currentTrackTitleChanged, videoWindow, &VideoWindow::setCurrentTrackTitle);
    connect(videoWindow, &VideoWindow::moveStarted, this, [this](bool detach) {
        beginAttachedWindowDrag(videoWindow, detach);
    });
    connect(videoWindow, &VideoWindow::moveFinished, this, [this] {
        finishAttachedWindowDrag(videoWindow);
    });

    // Restore only after the playlist view has subscribed to insertion and
    // current-index changes, so a fast local M3U load is painted immediately.
    connect(m_playlist, &QMediaPlaylist::loadFailed, this, [this] {
        if (m_cloudLoadPending) {
            m_cloudLoadPending = false;
            m_isLoadingPlaylist = false;
            m_cloudRestoreIndex = -1;
        }
        qWarning() << "Unable to restore saved playlist:" << m_playlist->errorString();
        if (m_cloudResumePending) {
            m_cloudResumePending = false;
            m_cloudPlaylistSync->publish(serializePlaylist());
            m_cloudPlaylistSync->resumeSavedProvider();
        }
    });
    QSettings().remove(QStringLiteral("playlist/cloudFile"));
    loadSavedPlaylist();
    if (!QFile::exists(defaultPlaylistPath())) {
        QTimer::singleShot(0, this, [this] {
            if (!m_cloudResumePending)
                return;
            m_cloudResumePending = false;
            m_cloudPlaylistSync->publish(serializePlaylist());
            m_cloudPlaylistSync->resumeSavedProvider();
        });
    }

    // The equalizer is also a separate classic-style window.
    m_equalizerWindow = new QDialog(this, Qt::Tool | Qt::FramelessWindowHint);
    m_equalizerWindow->installEventFilter(this);
    m_equalizerWindow->setWindowTitle(tr("Linkamp Equalizer"));
    m_equalizerWindow->setFixedSize(275, 116);
    const QString equalizerPositionKey = QStringLiteral("windows/equalizer/position");
    if (QSettings().contains(equalizerPositionKey)) {
        m_equalizerWindow->move(QSettings().value(equalizerPositionKey).toPoint());
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        m_haveEqualizerPosition = true;
    }
    const auto windowsAreAdjacent = [](const QWidget *first, const QWidget *second) {
        if (!first || !second)
            return false;
        const QRect a = first->frameGeometry();
        const QRect b = second->frameGeometry();
        constexpr int snapDistance = 12;
        const int horizontalOverlap = std::min(a.right(), b.right())
                                    - std::max(a.left(), b.left());
        const int verticalOverlap = std::min(a.bottom(), b.bottom())
                                  - std::max(a.top(), b.top());
        const bool verticallyAdjacent = horizontalOverlap >= 20
            && (qAbs(b.top() - a.bottom() - 1) <= snapDistance
                || qAbs(a.top() - b.bottom() - 1) <= snapDistance);
        const bool horizontallyAdjacent = verticalOverlap >= 20
            && (qAbs(b.left() - a.right() - 1) <= snapDistance
                || qAbs(a.left() - b.right() - 1) <= snapDistance);
        return verticallyAdjacent || horizontallyAdjacent;
    };
    if (m_haveEqualizerPosition)
        m_equalizerAttached = windowsAreAdjacent(m_mainPlayerWindow, m_equalizerWindow);
    if (m_havePlaylistPosition) {
        m_playlistAttached = windowsAreAdjacent(m_mainPlayerWindow, playlist)
            || (m_equalizerAttached && windowsAreAdjacent(m_equalizerWindow, playlist));
    }
    m_milkdropAttached = false;
    m_videoAttached = false;
    m_equalizerWindow->setStyleSheet("QDialog { background: transparent; border: 0px; }");
    auto *equalizerWindowLayout = new QVBoxLayout(m_equalizerWindow);
    equalizerWindowLayout->setContentsMargins(0, 0, 0, 0);
    equalizerWindowLayout->setSpacing(0);
    auto *equalizerTitleBar = new TitleBar(m_equalizerWindow);
    equalizerTitleBar->setClassicEqualizerStyle();
    equalizerTitleBar->setFixedHeight(14);
    connect(equalizerTitleBar, &TitleBar::moveStarted, this, [this](bool detach) {
        beginAttachedWindowDrag(m_equalizerWindow, detach);
    });
    connect(equalizerTitleBar, &TitleBar::moveFinished, this, [this] {
        finishAttachedWindowDrag(m_equalizerWindow);
    });
    connect(equalizerTitleBar, &TitleBar::shadeClicked, this, &MainWindow::toggleEqualizerWindowShade);
    equalizerWindowLayout->addWidget(equalizerTitleBar);
    equalizerView = new EqualizerView(equalizer, m_equalizerWindow);
    equalizerView->setAttribute(Qt::WidgetAttribute::WA_StyledBackground, true);
    equalizerWindowLayout->addWidget(equalizerView, 1);
    connect(equalizerView, &EqualizerView::backClicked, m_equalizerWindow, &QWidget::hide);

    // Prepare navigation stack
    viewStack = new QStackedLayout;
    viewStack->addWidget(playerWindow);

    // Final UI setup and show
    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->addLayout(viewStack);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    QWidget *centralWidget = new QWidget;
    centralWidget->setLayout(centralLayout);

    setCentralWidget(centralWidget);

    resize(WINDOW_W, WINDOW_H);
    this->setMaximumWidth(WINDOW_W);
    this->setMaximumHeight(WINDOW_H);
    this->setMinimumWidth(WINDOW_W);
    this->setMinimumHeight(WINDOW_H);

    #ifndef IS_EMBEDDED
    setWindowFlags(Qt::CustomizeWindowHint);
    #endif

    m_trayIcon = new QSystemTrayIcon(QIcon(QStringLiteral(":/assets/linkamp-mark.png")), this);
    m_trayIcon->setToolTip(QStringLiteral("Linkamp"));
    auto *trayMenu = new QMenu(this);
    QAction *openLinkamp = trayMenu->addAction(tr("Apri Linkamp"));
    connect(openLinkamp, &QAction::triggered, this, &MainWindow::showPlayer);
    trayMenu->addSeparator();
    QAction *exitLinkamp = trayMenu->addAction(tr("Esci"));
    connect(exitLinkamp, &QAction::triggered, qApp, &QCoreApplication::quit);
    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showPlayer();
    });
    if (QSystemTrayIcon::isSystemTrayAvailable())
        m_trayIcon->show();

    auto *updateManager = new UpdateManager(this);
    QTimer::singleShot(45000, updateManager, &UpdateManager::checkForUpdates);

    QTimer::singleShot(50, this, [this] {
        if (playlist && !playlist->isVisible())
            showPlaylist();
        if (milkdrop && QSettings().value(QStringLiteral("windows/milkdrop/visible"), true).toBool()
            && !milkdrop->isVisible())
            showMilkdrop();
        if (videoWindow && QSettings().value(QStringLiteral("windows/video/visible"), false).toBool()
            && !videoWindow->isVisible())
            showVideoWindow();
    });
    const QString savedSkin = QSettings().value(QStringLiteral("skin/classicWsz")).toString();
    if (!savedSkin.isEmpty()) {
        QString skinError;
        if (!WinampSkin::instance().load(savedSkin, &skinError)) {
            QSettings().remove(QStringLiteral("skin/classicWsz"));
            qWarning() << "Unable to restore Winamp skin:" << skinError;
        }
    }
    WinampSkin::instance().refreshWidgetStyles();
    if (player)
        player->refreshSkin();
    if (milkdrop)
        milkdrop->refreshSkin();
    if (videoWindow)
        videoWindow->refreshSkin();
}

MainWindow::~MainWindow()
{
    saveSavedPlaylist();
#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
    PyGILState_Ensure();
    Py_Finalize();
#endif
}

QString MainWindow::defaultPlaylistPath()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString dir = QDir(configDir).filePath(QCoreApplication::organizationName());
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("playlist.m3u8"));
}

void MainWindow::loadSavedPlaylist()
{
    const QString path = defaultPlaylistPath();
    if (!QFile::exists(path))
        return;

    m_isLoadingPlaylist = true;
    m_playlist->load(QUrl::fromLocalFile(path), "m3u8");
    m_isLoadingPlaylist = false;
}

void MainWindow::schedulePlaylistSave()
{
    if (m_isLoadingPlaylist)
        return;

    if (!m_playlistSaveTimer) {
        m_playlistSaveTimer = new QTimer(this);
        m_playlistSaveTimer->setSingleShot(true);
        m_playlistSaveTimer->setInterval(350);
        connect(m_playlistSaveTimer, &QTimer::timeout, this, &MainWindow::saveSavedPlaylist);
    }
    m_playlistSaveTimer->start();
}

void MainWindow::saveSavedPlaylist()
{
    if (m_isLoadingPlaylist || !m_playlist)
        return;

    if (m_playlistSaveTimer && m_playlistSaveTimer->isActive())
        m_playlistSaveTimer->stop();

    const QString path = defaultPlaylistPath();
    m_playlist->save(QUrl::fromLocalFile(path), "m3u8");
    m_cloudPlaylistSync->publish(serializePlaylist());
}

QByteArray MainWindow::serializePlaylist() const
{
    if (!m_playlist)
        return {};
    QByteArray data;
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly) || !m_playlist->save(&buffer, "m3u8"))
        return {};
    return data;
}

void MainWindow::applyCloudPlaylist(const QByteArray &data)
{
    if (data.isEmpty() || !data.trimmed().startsWith("#EXTM3U")) {
        QMessageBox::warning(this, tr("Playlist cloud"),
                             tr("Il file playlist nel cloud è vuoto o non è un M3U valido."));
        return;
    }

    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly))
        return;
    m_cloudRestoreIndex = m_playlist->currentIndex();
    m_cloudLoadPending = true;
    m_isLoadingPlaylist = true;
    m_playlist->clear();
    m_playlist->load(&buffer, "m3u8");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSavedPlaylist();
    if (playlist)
        playlist->close();
    if (m_equalizerWindow)
        m_equalizerWindow->close();
    if (milkdrop)
        milkdrop->close();
    if (videoWindow)
        videoWindow->close();
    QMainWindow::closeEvent(event);
}

void MainWindow::showPlayer()
{
    if (m_mainPlayerWindow) {
        m_mainPlayerWindow->showNormal();
        m_mainPlayerWindow->show();
        m_mainPlayerWindow->raise();
        m_mainPlayerWindow->activateWindow();
    } else {
        showNormal();
        show();
        raise();
        activateWindow();
    }
    if (m_minimizedWithMain) {
        m_minimizedWithMain = false;
        if (m_equalizerVisibleOnMinimize && m_equalizerWindow)
            m_equalizerWindow->show();
        if (m_playlistVisibleOnMinimize && playlist)
            playlist->show();
        if (m_milkdropVisibleOnMinimize && milkdrop)
            milkdrop->show();
        if (m_videoVisibleOnMinimize && videoWindow)
            videoWindow->show();
    }
}

void MainWindow::showPlaylist()
{
    if (!playlist)
        return;
    if (playlist->isVisible()) {
        playlist->hide();
        return;
    }
    if (!m_havePlaylistPosition) {
        playlist->resize(275, 116);
        m_syncingAttachedWindows = true;
        playlist->move(m_mainPlayerWindow->frameGeometry().left(),
                       windowBottomOnScreen(m_mainPlayerWindow));
        m_lastPlaylistPosition = playlist->pos();
        m_havePlaylistPosition = true;
        m_syncingAttachedWindows = false;
        m_playlistAttached = true;
    } else if (m_playlistAttached) {
        const int y = m_equalizerWindow && m_equalizerWindow->isVisible()
            ? windowBottomOnScreen(m_equalizerWindow)
            : windowBottomOnScreen(m_mainPlayerWindow);
        m_syncingAttachedWindows = true;
        playlist->move(m_mainPlayerWindow->frameGeometry().left(), y);
        m_lastPlaylistPosition = playlist->pos();
        m_syncingAttachedWindows = false;
    }
    playlist->show();
    playlist->raise();
    playlist->activateWindow();
}

void MainWindow::showMenu()
{
    QMenu menu(this);
    QAction *openFiles = menu.addAction(tr("Open file(s)…"));
    QAction *addYouTube = menu.addAction(tr("Add YouTube URL…"));
    menu.addSeparator();
    QAction *playlistAction = menu.addAction(tr("Playlist editor"));
    playlistAction->setCheckable(true);
    playlistAction->setChecked(playlist && playlist->isVisible());
    QAction *equalizerAction = menu.addAction(tr("Equalizer"));
    equalizerAction->setCheckable(true);
    equalizerAction->setChecked(m_equalizerWindow && m_equalizerWindow->isVisible());
    QAction *milkdropAction = menu.addAction(tr("MilkDrop"));
    milkdropAction->setCheckable(true);
    milkdropAction->setChecked(milkdrop && milkdrop->isVisible());
    QAction *videoAction = menu.addAction(tr("Video"));
    videoAction->setCheckable(true);
    videoAction->setChecked(videoWindow && videoWindow->isVisible());
    menu.addSeparator();
    QMenu *sharedPlaylistMenu = menu.addMenu(tr("Shared playlist"));
    QAction *googleDriveAction = sharedPlaylistMenu->addAction(tr("Connect Google Drive…"));
    QAction *oneDriveAction = sharedPlaylistMenu->addAction(tr("Connect OneDrive…"));
    QAction *disconnectSharedPlaylist = nullptr;
    if (!QSettings().value(QStringLiteral("cloud/provider")).toString().isEmpty())
        disconnectSharedPlaylist = sharedPlaylistMenu->addAction(tr("Disconnect cloud playlist"));
    menu.addSeparator();
    QMenu *skinMenu = menu.addMenu(tr("Skins"));
    const QString skinDirectory = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(QStringLiteral("skins"));
    QDir().mkpath(skinDirectory);
    const QFileInfoList availableSkinFiles = QDir(skinDirectory).entryInfoList(
        {QStringLiteral("*.wsz"), QStringLiteral("*.zip")}, QDir::Files, QDir::Name);
    QFileInfoList installedSkins;
    for (const QFileInfo &skinFile : availableSkinFiles) {
        // The bundled Classic is the default already; don't offer an identical
        // separately-installed copy as a second choice.
        if (skinFile.completeBaseName().compare(QStringLiteral("Winamp Classic"),
                                                Qt::CaseInsensitive) != 0)
            installedSkins.append(skinFile);
    }
    for (const QFileInfo &skinFile : installedSkins) {
        QAction *skinAction = skinMenu->addAction(skinFile.completeBaseName());
        skinAction->setData(skinFile.absoluteFilePath());
    }
    if (!installedSkins.isEmpty())
        skinMenu->addSeparator();
    QAction *loadSkinAction = skinMenu->addAction(tr("Select skin file…"));
    QAction *loadSkinFolderAction = skinMenu->addAction(tr("Select extracted skin folder…"));
    skinMenu->addSeparator();
    QAction *defaultSkinAction = skinMenu->addAction(tr("Classic (Linkamp default)"));
    menu.addSeparator();
    QAction *fileSourceAction = menu.addAction(tr("File source"));
#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
    QAction *cdSourceAction = menu.addAction(tr("CD source"));
    QAction *bluetoothSourceAction = menu.addAction(tr("Bluetooth source"));
    QAction *spotifySourceAction = menu.addAction(tr("Spotify source"));
#endif

    QAction *selected = menu.exec(QCursor::pos());
    if (selected == openFiles)
        open();
    else if (selected == addYouTube)
        promptForYouTubeUrl();
    else if (selected == playlistAction)
        showPlaylist();
    else if (selected == equalizerAction)
        showEqualizer();
    else if (selected == milkdropAction)
        showMilkdrop();
    else if (selected == videoAction)
        showVideoWindow();
    else if (selected == googleDriveAction || selected == oneDriveAction) {
        QSettings().remove(QStringLiteral("playlist/cloudFile"));
        m_cloudPlaylistSync->publish(serializePlaylist());
        m_cloudPlaylistSync->connectProvider(selected == googleDriveAction
            ? CloudPlaylistSync::Provider::GoogleDrive
            : CloudPlaylistSync::Provider::OneDrive);
    } else if (selected == disconnectSharedPlaylist) {
        m_cloudPlaylistSync->disconnectProvider();
        QSettings().remove(QStringLiteral("playlist/cloudFile"));
    }
    else if (selected == loadSkinAction) {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Load Winamp classic skin"), QString(),
            tr("Winamp classic skins (*.wsz *.zip);;All files (*)"));
        if (!path.isEmpty()) {
            QString skinError;
            if (!WinampSkin::instance().load(path, &skinError))
                QMessageBox::warning(this, tr("Skin non caricata"), skinError);
            else
                refreshAttachedWindowPositions();
        }
    } else if (selected == loadSkinFolderAction) {
        const QString path = QFileDialog::getExistingDirectory(
            this, tr("Load extracted Winamp skin folder"), QDir::homePath());
        if (!path.isEmpty()) {
            QString skinError;
            if (!WinampSkin::instance().load(path, &skinError))
                QMessageBox::warning(this, tr("Skin non caricata"), skinError);
            else
                refreshAttachedWindowPositions();
        }
    } else if (selected == defaultSkinAction) {
        WinampSkin::instance().reset();
        refreshAttachedWindowPositions();
    } else if (selected && selected->data().canConvert<QString>()
               && !selected->data().toString().isEmpty()) {
        QString skinError;
        if (!WinampSkin::instance().load(selected->data().toString(), &skinError))
            QMessageBox::warning(this, tr("Skin non caricata"), skinError);
        else
            refreshAttachedWindowPositions();
    }
    else if (selected == fileSourceAction)
        coordinator->setSource(0);
#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
    else if (selected == bluetoothSourceAction)
        coordinator->setSource(1);
    else if (selected == cdSourceAction)
        coordinator->setSource(2);
    else if (selected == spotifySourceAction)
        coordinator->setSource(3);
#endif
}

void MainWindow::showEqualizer()
{
    if (!m_equalizerWindow)
        return;
    if (m_equalizerWindow->isVisible()) {
        m_equalizerWindow->hide();
        if (playlist && playlist->isVisible() && m_playlistAttached && m_equalizerAttached) {
            m_syncingAttachedWindows = true;
            playlist->move(m_mainPlayerWindow->frameGeometry().left(),
                           windowBottomOnScreen(m_mainPlayerWindow));
            m_lastPlaylistPosition = playlist->pos();
            m_syncingAttachedWindows = false;
        }
        return;
    }
    if (!m_haveEqualizerPosition) {
        m_syncingAttachedWindows = true;
        m_equalizerWindow->move(m_mainPlayerWindow->frameGeometry().left(),
                                windowBottomOnScreen(m_mainPlayerWindow));
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        m_haveEqualizerPosition = true;
        m_syncingAttachedWindows = false;
        m_equalizerAttached = true;
    } else if (m_equalizerAttached) {
        m_syncingAttachedWindows = true;
        m_equalizerWindow->move(m_mainPlayerWindow->frameGeometry().left(),
                                windowBottomOnScreen(m_mainPlayerWindow));
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        if (playlist && playlist->isVisible() && m_playlistAttached) {
            playlist->move(m_mainPlayerWindow->frameGeometry().left(),
                           windowBottomOnScreen(m_equalizerWindow));
            m_lastPlaylistPosition = playlist->pos();
        }
        m_syncingAttachedWindows = false;
    }
    m_equalizerWindow->show();
    m_equalizerWindow->raise();
    m_equalizerWindow->activateWindow();
}

void MainWindow::showMilkdrop()
{
    if (!milkdrop)
        return;
    if (milkdrop->isVisible()) {
        QSettings().setValue(QStringLiteral("windows/milkdrop/visible"), false);
        milkdrop->hide();
        return;
    }
    QSettings().setValue(QStringLiteral("windows/milkdrop/visible"), true);
    const auto pickMilkdropAnchor = [this]() -> const QWidget * {
        const QRect mainRect = m_mainPlayerWindow->frameGeometry();
        const QWidget *anchor = m_mainPlayerWindow;
        if (m_equalizerWindow && m_equalizerWindow->isVisible()
            && qAbs(m_equalizerWindow->frameGeometry().left() - mainRect.left()) <= 16) {
            anchor = m_equalizerWindow;
        }
        if (playlist && playlist->isVisible()
            && qAbs(playlist->frameGeometry().left() - mainRect.left()) <= 16
            && windowBottomOnScreen(playlist) + milkdrop->height() <= 1000) {
            anchor = playlist;
        }
        return anchor;
    };
    if (!m_haveMilkdropPosition || m_lastMilkdropPosition.y() + milkdrop->height() > 1020) {
        const QWidget *anchor = pickMilkdropAnchor();
        m_syncingAttachedWindows = true;
        milkdrop->resize(275, 203);
        milkdrop->move(m_mainPlayerWindow->frameGeometry().left(),
                       windowBottomOnScreen(anchor));
        m_lastMilkdropPosition = milkdrop->pos();
        m_haveMilkdropPosition = true;
        m_syncingAttachedWindows = false;
        m_milkdropAttached = false;
    }
    milkdrop->show();
    milkdrop->raise();
    milkdrop->activateWindow();
}

void MainWindow::showVideoWindow()
{
    if (!videoWindow)
        return;
    if (videoWindow->isVisible()) {
        QSettings().setValue(QStringLiteral("windows/video/visible"), false);
        videoWindow->hide();
        return;
    }
    QSettings().setValue(QStringLiteral("windows/video/visible"), true);
    const auto pickVideoAnchor = [this]() -> const QWidget * {
        const QRect mainRect = m_mainPlayerWindow->frameGeometry();
        const QWidget *anchor = m_mainPlayerWindow;
        if (m_equalizerWindow && m_equalizerWindow->isVisible()
            && qAbs(m_equalizerWindow->frameGeometry().left() - mainRect.left()) <= 16) {
            anchor = m_equalizerWindow;
        }
        if (playlist && playlist->isVisible()
            && qAbs(playlist->frameGeometry().left() - mainRect.left()) <= 16
            && windowBottomOnScreen(playlist) + videoWindow->height() <= 1000) {
            anchor = playlist;
        }
        if (milkdrop && milkdrop->isVisible() && !milkdrop->isFullScreenMode()
            && qAbs(milkdrop->frameGeometry().left() - mainRect.left()) <= 16
            && windowBottomOnScreen(milkdrop) + videoWindow->height() <= 1000) {
            anchor = milkdrop;
        }
        return anchor;
    };
    if (!m_haveVideoPosition || m_lastVideoPosition.y() + videoWindow->height() > 1020) {
        const QWidget *anchor = pickVideoAnchor();
        m_syncingAttachedWindows = true;
        videoWindow->resize(275, 203);
        const int nextY = windowBottomOnScreen(anchor);
        if (nextY + videoWindow->height() <= 1020) {
            videoWindow->move(m_mainPlayerWindow->frameGeometry().left(), nextY);
        } else {
            videoWindow->move(m_mainPlayerWindow->frameGeometry().right() + 1,
                              m_mainPlayerWindow->frameGeometry().top());
        }
        m_lastVideoPosition = videoWindow->pos();
        m_haveVideoPosition = true;
        m_syncingAttachedWindows = false;
        m_videoAttached = false;
    }
    videoWindow->show();
    videoWindow->raise();
    videoWindow->activateWindow();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    m_lastMainPosition = m_mainPlayerWindow ? m_mainPlayerWindow->pos() : pos();
    m_haveMainPosition = true;
    if (m_equalizerWindow != nullptr && !m_haveEqualizerPosition) {
        m_syncingAttachedWindows = true;
        m_equalizerWindow->move(m_mainPlayerWindow->frameGeometry().left(),
                                windowBottomOnScreen(m_mainPlayerWindow));
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        m_haveEqualizerPosition = true;
        m_syncingAttachedWindows = false;
    }
    if (playlist != nullptr && !m_havePlaylistPosition) {
        const int y = (m_equalizerWindow != nullptr && m_equalizerWindow->isVisible())
            ? windowBottomOnScreen(m_equalizerWindow)
            : windowBottomOnScreen(m_mainPlayerWindow);
        m_syncingAttachedWindows = true;
        playlist->move(m_mainPlayerWindow->frameGeometry().left(), y);
        m_lastPlaylistPosition = playlist->pos();
        m_havePlaylistPosition = true;
        m_syncingAttachedWindows = false;
    }
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    const QPoint current = pos();
    if (m_syncingAttachedWindows || !m_haveMainPosition) {
        m_lastMainPosition = current;
        m_haveMainPosition = true;
        return;
    }
    const QPoint delta = current - m_lastMainPosition;
    m_lastMainPosition = current;
    moveAttachedWindows(this, delta);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (auto *mainTitleBar = findChild<TitleBar *>(QStringLiteral("titlebarContainer"))) {
            mainTitleBar->update();
        }
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *window = qobject_cast<QWidget *>(watched);
    if (window != nullptr && window == m_mainPlayerWindow
        && event->type() == QEvent::WindowStateChange) {
        if (m_mainPlayerWindow->isMinimized()) {
            if (!m_minimizedWithMain) {
                m_minimizedWithMain = true;
                m_playlistVisibleOnMinimize = playlist && playlist->isVisible();
                m_equalizerVisibleOnMinimize = m_equalizerWindow && m_equalizerWindow->isVisible();
                m_milkdropVisibleOnMinimize = milkdrop && milkdrop->isVisible();
                m_videoVisibleOnMinimize = videoWindow && videoWindow->isVisible();
                if (m_playlistVisibleOnMinimize)
                    playlist->hide();
                if (m_equalizerVisibleOnMinimize)
                    m_equalizerWindow->hide();
                if (m_milkdropVisibleOnMinimize)
                    milkdrop->hide();
                if (m_videoVisibleOnMinimize)
                    videoWindow->hide();
            }
        } else if (m_minimizedWithMain) {
            m_minimizedWithMain = false;
            if (m_equalizerVisibleOnMinimize && m_equalizerWindow)
                m_equalizerWindow->show();
            if (m_playlistVisibleOnMinimize && playlist)
                playlist->show();
            if (m_milkdropVisibleOnMinimize && milkdrop)
                milkdrop->show();
            if (m_videoVisibleOnMinimize && videoWindow)
                videoWindow->show();
        }
    }
    if (window != nullptr && m_mainPlayerWindow
        && (window == m_mainPlayerWindow || m_mainPlayerWindow->isAncestorOf(window))
        ) {
        if (event->type() == QEvent::ContextMenu) {
            showMenu();
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease
            && static_cast<QMouseEvent *>(event)->button() == Qt::RightButton) {
            showMenu();
            return true;
        }
    }
    if (window != nullptr && event->type() == QEvent::Move
        && (window == m_mainPlayerWindow || window == playlist
            || window == m_equalizerWindow
            || (window == milkdrop && !milkdrop->isFullScreenMode())
            || (window == videoWindow && !videoWindow->isFullScreenMode()))) {
        QPoint *lastPosition = window == m_mainPlayerWindow ? &m_lastMainPosition
            : window == playlist ? &m_lastPlaylistPosition
            : window == m_equalizerWindow ? &m_lastEqualizerPosition
            : window == milkdrop ? &m_lastMilkdropPosition : &m_lastVideoPosition;
        bool *hasPosition = window == m_mainPlayerWindow ? &m_haveMainPosition
            : window == playlist ? &m_havePlaylistPosition
            : window == m_equalizerWindow ? &m_haveEqualizerPosition
            : window == milkdrop ? &m_haveMilkdropPosition : &m_haveVideoPosition;
        const QPoint current = window->pos();
        if (window->isVisible()) {
            const QString key = window == m_mainPlayerWindow
                ? QStringLiteral("windows/main/position")
                : window == m_equalizerWindow
                    ? QStringLiteral("windows/equalizer/position")
                    : window == milkdrop
                        ? QStringLiteral("windows/milkdrop/position")
                        : window == videoWindow
                            ? QStringLiteral("windows/video/position")
                            : QStringLiteral("windows/playlist/position");
            QSettings().setValue(key, window->frameGeometry().topLeft());
        }
        if (m_syncingAttachedWindows || !*hasPosition) {
            *lastPosition = current;
            *hasPosition = true;
            return QMainWindow::eventFilter(watched, event);
        }
        const QPoint delta = current - *lastPosition;
        *lastPosition = current;
        moveAttachedWindows(window, delta);
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::moveAttachedWindows(QWidget *source, const QPoint &delta)
{
    if (m_syncingAttachedWindows || delta.isNull())
        return;

    if (source == milkdrop || source == videoWindow)
        return;
    if (source == playlist && !m_playlistAttached)
        return;
    if (source == m_equalizerWindow && !m_equalizerAttached)
        return;

    m_syncingAttachedWindows = true;
    if (source != this)
        move(pos() + delta);
    if (playlist != nullptr && source != playlist && m_playlistAttached)
        playlist->move(playlist->pos() + delta);
    if (m_equalizerWindow != nullptr && source != m_equalizerWindow && m_equalizerAttached)
        m_equalizerWindow->move(m_equalizerWindow->pos() + delta);
    m_lastMainPosition = m_mainPlayerWindow ? m_mainPlayerWindow->pos() : pos();
    m_lastPlaylistPosition = playlist != nullptr ? playlist->pos() : QPoint();
    m_lastEqualizerPosition = m_equalizerWindow != nullptr ? m_equalizerWindow->pos() : QPoint();
    m_haveMainPosition = true;
    m_havePlaylistPosition = playlist != nullptr;
    m_haveEqualizerPosition = m_equalizerWindow != nullptr;
    m_syncingAttachedWindows = false;
}

void MainWindow::refreshAttachedWindowPositions()
{
    if (!m_mainPlayerWindow)
        return;
    if (m_equalizerWindow) {
        m_equalizerWindow->updateGeometry();
    }
    if (player)
        player->refreshSkin();
    if (milkdrop)
        milkdrop->refreshSkin();
    if (videoWindow)
        videoWindow->refreshSkin();

    // A skin can change child size hints and the window manager then adjusts
    // top-level positions during polish. Reapply the established vertical
    // stack after those layout events, while retaining each attachment flag.
    QTimer::singleShot(0, this, [this] {
        if (!m_mainPlayerWindow)
            return;
        m_syncingAttachedWindows = true;
        const int left = m_mainPlayerWindow->frameGeometry().left();
        if (m_equalizerWindow && m_equalizerAttached) {
            m_equalizerWindow->move(left, windowBottomOnScreen(m_mainPlayerWindow));
            if (playlist && m_playlistAttached && playlist->isVisible())
                playlist->move(left, windowBottomOnScreen(m_equalizerWindow));
        } else if (playlist && m_playlistAttached && playlist->isVisible()) {
            playlist->move(left, windowBottomOnScreen(m_mainPlayerWindow));
        }
        m_lastMainPosition = m_mainPlayerWindow->pos();
        m_haveMainPosition = true;
        if (playlist) {
            m_lastPlaylistPosition = playlist->pos();
            m_havePlaylistPosition = true;
        }
        if (m_equalizerWindow) {
            m_lastEqualizerPosition = m_equalizerWindow->pos();
            m_haveEqualizerPosition = true;
        }
        m_syncingAttachedWindows = false;
    });
}

void MainWindow::beginAttachedWindowDrag(QWidget *window, bool detach)
{
    if (window == m_mainPlayerWindow || window == this) {
        m_mainForceDetach = detach;
        if (detach) {
            m_playlistAttached = false;
            m_equalizerAttached = false;
            m_milkdropAttached = false;
            m_videoAttached = false;
        }
    } else if (window == playlist) {
        m_playlistForceDetach = detach;
        if (detach)
            m_playlistAttached = false;
    } else if (window == m_equalizerWindow) {
        m_equalizerForceDetach = detach;
        if (detach)
            m_equalizerAttached = false;
    } else if (window == milkdrop) {
        m_milkdropForceDetach = detach;
        if (detach)
            m_milkdropAttached = false;
    } else if (window == videoWindow) {
        m_videoForceDetach = detach;
        if (detach)
            m_videoAttached = false;
    }
}

void MainWindow::finishAttachedWindowDrag(QWidget *window)
{
    if (window == m_mainPlayerWindow || window == this) {
        const bool forced = m_mainForceDetach;
        m_mainForceDetach = false;
        if (forced)
            return;
        if (!m_playlistAttached && playlist != nullptr)
            m_playlistAttached = snapWindowToMain(playlist);
        if (!m_equalizerAttached && m_equalizerWindow != nullptr)
            m_equalizerAttached = snapWindowToMain(m_equalizerWindow);
        return;
    }
    if (window == playlist) {
        const bool forced = m_playlistForceDetach;
        m_playlistForceDetach = false;
        if (!forced && !m_playlistAttached)
            m_playlistAttached = snapWindowToMain(playlist);
        return;
    }
    if (window == m_equalizerWindow) {
        const bool forced = m_equalizerForceDetach;
        m_equalizerForceDetach = false;
        if (!forced && !m_equalizerAttached)
            m_equalizerAttached = snapWindowToMain(m_equalizerWindow);
        return;
    }
    if (window == milkdrop) {
        m_milkdropForceDetach = false;
        m_milkdropAttached = false;
        return;
    }
    if (window == videoWindow) {
        m_videoForceDetach = false;
        m_videoAttached = false;
    }
}

bool MainWindow::snapWindowToMain(QWidget *window)
{
    if (window == nullptr)
        return false;

    const QRect mainRect = m_mainPlayerWindow
        ? m_mainPlayerWindow->frameGeometry() : frameGeometry();
    const QRect childRect = window->frameGeometry();
    constexpr int snapDistance = 12;
    struct SnapCandidate {
        int distance;
        QPoint position;
    };
    QList<SnapCandidate> candidates;

    const int verticalOverlap = std::min(mainRect.bottom(), childRect.bottom())
                              - std::max(mainRect.top(), childRect.top());
    const int horizontalOverlap = std::min(mainRect.right(), childRect.right())
                                - std::max(mainRect.left(), childRect.left());
    if (verticalOverlap >= 20) {
        const int rightDistance = qAbs(childRect.left() - mainRect.right() - 1);
        if (rightDistance <= snapDistance)
            candidates.append({rightDistance, QPoint(mainRect.right() + 1, childRect.top())});
        const int leftDistance = qAbs(childRect.right() + 1 - mainRect.left());
        if (leftDistance <= snapDistance)
            candidates.append({leftDistance, QPoint(mainRect.left() - childRect.width(), childRect.top())});
    }
    if (horizontalOverlap >= 30) {
        const int belowDistance = qAbs(childRect.top() - mainRect.bottom() - 1);
        if (belowDistance <= snapDistance)
            candidates.append({belowDistance, QPoint(childRect.left(), mainRect.bottom() + 1)});
        const int aboveDistance = qAbs(childRect.bottom() + 1 - mainRect.top());
        if (aboveDistance <= snapDistance)
            candidates.append({aboveDistance, QPoint(childRect.left(), mainRect.top() - childRect.height())});
    }
    if (candidates.isEmpty())
        return false;

    const auto closest = std::min_element(candidates.cbegin(), candidates.cend(),
        [](const SnapCandidate &a, const SnapCandidate &b) { return a.distance < b.distance; });
    m_syncingAttachedWindows = true;
    window->move(closest->position);
    m_syncingAttachedWindows = false;
    if (window == playlist) {
        m_lastPlaylistPosition = playlist->pos();
        m_havePlaylistPosition = true;
    } else if (window == m_equalizerWindow) {
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        m_haveEqualizerPosition = true;
    } else if (window == milkdrop) {
        m_lastMilkdropPosition = milkdrop->pos();
        m_haveMilkdropPosition = true;
    } else if (window == videoWindow) {
        m_lastVideoPosition = videoWindow->pos();
        m_haveVideoPosition = true;
    }
    return true;
}

void MainWindow::toggleWindowShade()
{
#ifndef IS_EMBEDDED
    if (m_playerBodyContainer == nullptr)
        return;
    if (!m_windowShaded) {
        m_restoreWindowHeight = height();
        m_playerBodyContainer->hide();
        setMinimumHeight(14);
        setMaximumHeight(14);
        resize(WINDOW_W, 14);
    } else {
        m_playerBodyContainer->show();
        setMinimumHeight(WINDOW_H);
        setMaximumHeight(WINDOW_H);
        resize(WINDOW_W, m_restoreWindowHeight);
    }
    m_windowShaded = !m_windowShaded;
#endif
}

void MainWindow::toggleEqualizerWindowShade()
{
    if (m_equalizerWindow == nullptr || equalizerView == nullptr)
        return;

    if (!m_equalizerWindowShaded) {
        m_restoreEqualizerWindowHeight = m_equalizerWindow->height();
        equalizerView->hide();
        m_equalizerWindow->setFixedHeight(14);
        m_equalizerWindowShaded = true;
    } else {
        m_equalizerWindow->setFixedHeight(m_restoreEqualizerWindowHeight);
        equalizerView->show();
        m_equalizerWindowShaded = false;
    }
}

void MainWindow::showShutdownModal()
{
    QMessageBox msgBox;
    msgBox.setText("Shutdown");
    msgBox.setInformativeText("Turn off Linkamp?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setWindowFlags(Qt::FramelessWindowHint);
    int ret = msgBox.exec();

    switch (ret) {
    case QMessageBox::Yes:
        shutdown();
        break;
    default:
        break;
    }
}

void MainWindow::shutdown()
{
    QString appPath = QCoreApplication::applicationDirPath();
    QString cmd = appPath + "/shutdown.sh";

    shutdownProcess = new QProcess(this);
    shutdownProcess->start(cmd);
}

// Shows a standard file picker for adding items to the playlist
// Not used currently
void MainWindow::open()
{
    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first())
         << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first())
         << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::MusicLocation).first());


    QFileDialog fileDialog(this);
    QString filters = audioFileFilters().join(" ");
    fileDialog.setNameFilter("Audio (" + filters + ")");
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setWindowTitle(tr("Open Files"));
    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::MusicLocation)
                                .value(0, QDir::homePath()));
    fileDialog.setOption(QFileDialog::ReadOnly, true);
    fileDialog.setViewMode(QFileDialog::Detail);
    fileDialog.setSidebarUrls(urls);

#ifdef IS_EMBEDDED
    fileDialog.setWindowState(Qt::WindowFullScreen);
#endif

    if (fileDialog.exec() == QDialog::Accepted) {
        coordinator->setSource(0);
        fileSource->addToPlaylist(fileDialog.selectedUrls());
    }

}

void MainWindow::addFolderToPlaylist()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, tr("Add folder to playlist"),
        QStandardPaths::standardLocations(QStandardPaths::MusicLocation)
            .value(0, QDir::homePath()));
    if (directory.isEmpty())
        return;

    QDirIterator iterator(directory, audioFileFilters(), QDir::Files, QDirIterator::Subdirectories);
    QList<QUrl> urls;
    while (iterator.hasNext())
        urls.append(QUrl::fromLocalFile(iterator.next()));
    if (!urls.isEmpty()) {
        coordinator->setSource(0);
        fileSource->addToPlaylist(urls);
    }
}

void MainWindow::promptForYouTubeUrl()
{
    QDialog dialog(this, Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setWindowTitle(tr("Open URL"));
    dialog.setFixedSize(186, 68);
    dialog.setStyleSheet("QDialog { background:#20202c; border:1px solid #727284; }"
                         "QLabel { color:#e8e8ef; }"
                         "QLineEdit { background:#fff; color:#111; border:1px solid #55556a; }");

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(7, 5, 7, 5);
    layout->setSpacing(2);
    auto *label = new QLabel(tr("YouTube URL:"), &dialog);
    auto *edit = new QLineEdit(&dialog);
    edit->setPlaceholderText(QStringLiteral("https://youtu.be/…"));
    edit->setFixedHeight(16);
    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addStretch(1);
    auto *addButton = new QPushButton(tr("Add"), &dialog);
    auto *cancelButton = new QPushButton(tr("Cancel"), &dialog);
    addButton->setFixedSize(50, 14);
    cancelButton->setFixedSize(50, 14);
    buttons->addWidget(addButton);
    buttons->addWidget(cancelButton);
    layout->addWidget(label);
    layout->addWidget(edit);
    layout->addLayout(buttons);
    connect(addButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(edit, &QLineEdit::returnPressed, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted && !edit->text().trimmed().isEmpty())
        addYouTubeUrl(edit->text());
}

void MainWindow::addYouTubeUrl(const QString &rawUrl)
{
    const QUrl url = QUrl::fromUserInput(rawUrl.trimmed());
    if (!isYouTubeUrl(url)) {
        QMessageBox::warning(this, tr("URL non valido"),
                             tr("Inserisci un link a un video YouTube o youtu.be."));
        return;
    }

    const QString videoId = youtubeVideoId(url);
    for (int index = 0; index < m_playlist->mediaCount(); ++index) {
        const QUrl existingUrl = m_playlist->media(index);
        if ((!videoId.isEmpty() && youtubeVideoId(existingUrl) == videoId)
            || (videoId.isEmpty() && existingUrl == url)) {
            if (playlist) {
                if (!playlist->isVisible())
                    showPlaylist();
                playlist->raise();
                playlist->activateWindow();
            }
            return;
        }
    }
    if (!videoId.isEmpty() && m_pendingYouTubeIds.contains(videoId))
        return;

    const QString ytDlp = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (ytDlp.isEmpty()) {
        QMessageBox::warning(this, tr("yt-dlp non trovato"),
                             tr("Installa yt-dlp nell'ambiente Python di Linkamp e riprova."));
        return;
    }

    if (!videoId.isEmpty())
        m_pendingYouTubeIds.insert(videoId);

    auto *progress = new QProgressDialog(tr("Recupero titolo e informazioni da YouTube…"),
                                         tr("Annulla"), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(250);
    progress->setAutoClose(false);
    progress->setAutoReset(false);

    auto *process = new QProcess(this);
    connect(progress, &QProgressDialog::canceled, process, &QProcess::kill);
    connect(process, &QProcess::errorOccurred, this,
            [this, process, progress, videoId](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_pendingYouTubeIds.remove(videoId);
        progress->close();
        progress->deleteLater();
        QMessageBox::warning(this, tr("Errore yt-dlp"), tr("Impossibile avviare yt-dlp."));
        process->deleteLater();
    });
    connect(process, &QProcess::finished, this,
            [this, process, progress, url, videoId](int exitCode, QProcess::ExitStatus) {
        const QByteArray output = process->readAllStandardOutput();
        const QString errorOutput = QString::fromUtf8(process->readAllStandardError()).trimmed();
        const bool cancelled = progress->wasCanceled();
        m_pendingYouTubeIds.remove(videoId);
        progress->close();
        progress->deleteLater();
        process->deleteLater();

        if (cancelled)
            return;
        if (exitCode != 0) {
            QMessageBox::warning(this, tr("Errore YouTube"),
                                 errorOutput.isEmpty() ? tr("Non riesco a leggere questo video.")
                                                       : errorOutput.left(500));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            QMessageBox::warning(this, tr("Errore YouTube"),
                                 tr("yt-dlp non ha restituito i dati del video."));
            return;
        }

        const QJsonObject info = document.object();
        const QString title = info.value(QStringLiteral("title")).toString().trimmed();
        if (title.isEmpty()) {
            QMessageBox::warning(this, tr("Video senza titolo"),
                                 tr("Non è stato possibile leggere il titolo del video."));
            return;
        }

        QUrl playlistUrl(info.value(QStringLiteral("webpage_url")).toString());
        if (!isYouTubeUrl(playlistUrl))
            playlistUrl = url;

        QMediaMetaData metadata;
        metadata.insert(QMediaMetaData::Title, title);
        metadata.insert(QMediaMetaData::Url, playlistUrl);
        const QString artist = info.value(QStringLiteral("channel")).toString(
            info.value(QStringLiteral("uploader")).toString());
        if (!artist.isEmpty())
            metadata.insert(QMediaMetaData::AlbumArtist, artist);
        if (info.value(QStringLiteral("duration")).isDouble())
            metadata.insert(QMediaMetaData::Duration,
                            qint64(info.value(QStringLiteral("duration")).toDouble() * 1000.0));

        fileSource->addYouTubeToPlaylist(playlistUrl, metadata);
        if (!playlist->isVisible())
            showPlaylist();
        playlist->raise();
        playlist->activateWindow();
        raise();
        activateWindow();
    });

    process->start(ytDlp, {
        QStringLiteral("--no-playlist"), QStringLiteral("--no-warnings"),
        QStringLiteral("--quiet"), QStringLiteral("--skip-download"),
        QStringLiteral("--dump-single-json"), QStringLiteral("--"),
        url.toString(QUrl::FullyEncoded)
    });
}

void MainWindow::installSkinFromUrl(const QUrl &url, const QString &requestedFileName)
{
    QHostAddress hostAddress;
    const QString host = url.host().toLower();
    const bool localHost = host == QLatin1String("localhost")
        || host.endsWith(QStringLiteral(".localhost"))
        || host.endsWith(QStringLiteral(".local"))
        || hostAddress.setAddress(host);
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
        || url.path().endsWith(QStringLiteral(".wsz"), Qt::CaseInsensitive) == false
        || url.userInfo().size() != 0 || (url.port(-1) != -1 && url.port(-1) != 443)
        || host.isEmpty() || localHost) {
        QMessageBox::warning(this, tr("Skin non valida"),
                             tr("Il link deve puntare a un file .wsz HTTPS pubblico."));
        return;
    }

    auto *network = new QNetworkAccessManager(this);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Linkamp skin installer"));
    QNetworkReply *reply = network->get(request);
    const QString downloadedFileName = QFileInfo(requestedFileName).fileName();
    auto *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    timeout->start(30000);
    connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::downloadProgress, reply,
            [reply](qint64 received, qint64 total) {
        constexpr qint64 maxSkinBytes = 64LL * 1024 * 1024;
        if (received > maxSkinBytes || total > maxSkinBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, network, timeout,
                                                     downloadedFileName, url] {
        timeout->stop();
        const QByteArray archiveData = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();
        network->deleteLater();
        if (networkError != QNetworkReply::NoError || archiveData.isEmpty()
            || archiveData.size() > 64 * 1024 * 1024) {
            QMessageBox::warning(this, tr("Download skin non riuscito"),
                                 networkError == QNetworkReply::NoError
                                     ? tr("Il file skin è vuoto o supera 64 MB.")
                                     : networkErrorText);
            return;
        }

        QString fileName = downloadedFileName;
        if (fileName.isEmpty() || !fileName.endsWith(QStringLiteral(".wsz"), Qt::CaseInsensitive))
            fileName = QStringLiteral("%1.wsz").arg(QFileInfo(url.path()).completeBaseName());
        fileName.replace(QLatin1Char('\\'), QLatin1Char('/'));
        fileName = QFileInfo(fileName).fileName();
        for (qsizetype index = 0; index < fileName.size(); ++index) {
            const QChar character = fileName.at(index);
            if (!character.isLetterOrNumber() && character != QLatin1Char('.')
                && character != QLatin1Char('_') && character != QLatin1Char('-')
                && character != QLatin1Char(' '))
                fileName[index] = QLatin1Char('_');
        }
        while (fileName.startsWith(QLatin1Char('.')))
            fileName.remove(0, 1);
        if (fileName.isEmpty() || !fileName.endsWith(QStringLiteral(".wsz"), Qt::CaseInsensitive))
            fileName = QStringLiteral("%1.wsz").arg(QFileInfo(url.path()).completeBaseName());
        const QString skinDirectory = QDir(QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation)).filePath(QStringLiteral("skins"));
        if (!QDir().mkpath(skinDirectory)) {
            QMessageBox::warning(this, tr("Installazione skin non riuscita"),
                                 tr("Non riesco a creare la cartella delle skin."));
            return;
        }
        const QString skinPath = QDir(skinDirectory).filePath(fileName);
        const QString stagingPath = skinPath + QStringLiteral(".download");
        QSaveFile stagingFile(stagingPath);
        if (!stagingFile.open(QIODevice::WriteOnly)
            || stagingFile.write(archiveData) != archiveData.size()
            || !stagingFile.commit()) {
            stagingFile.cancelWriting();
            QMessageBox::warning(this, tr("Installazione skin non riuscita"),
                                 tr("Non riesco a salvare il file skin."));
            return;
        }

        QString skinError;
        if (!WinampSkin::instance().load(stagingPath, &skinError)) {
            QFile::remove(stagingPath);
            QMessageBox::warning(this, tr("Skin non caricata"), skinError);
            return;
        }
        QSaveFile installedFile(skinPath);
        if (!installedFile.open(QIODevice::WriteOnly)
            || installedFile.write(archiveData) != archiveData.size()
            || !installedFile.commit()) {
            installedFile.cancelWriting();
            QMessageBox::warning(this, tr("Installazione skin incompleta"),
                                 tr("La skin è stata caricata, ma non riesco a installarla nella cartella delle skin."));
            return;
        }
        if (!WinampSkin::instance().load(skinPath, &skinError)) {
            QFile::remove(stagingPath);
            QMessageBox::warning(this, tr("Skin non caricata"), skinError);
            return;
        }
        QFile::remove(stagingPath);
        refreshAttachedWindowPositions();
        QMessageBox::information(this, tr("Skin installata"),
                                 tr("La skin %1 è stata installata e applicata.").arg(fileName));
    });
}
