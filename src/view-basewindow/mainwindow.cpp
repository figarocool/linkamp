#include <QFileDialog>
#include <QDirIterator>
#include <QCloseEvent>
#include <QTimer>
#include <QDir>
#include <QFile>

#include "mainwindow.h"
#include "desktopplayerwindow.h"
#include "titlebar.h"
#include "qstandardpaths.h"
#include "ui_desktopplayerwindow.h"
#include "scale.h"
#include "util.h"

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

#include <algorithm>

#ifdef IS_EMBEDDED
const unsigned int WINDOW_W = 320 * UI_SCALE;
const unsigned int WINDOW_H = 100 * UI_SCALE;
#else
const unsigned int WINDOW_W = 275 * UI_SCALE;
const unsigned int WINDOW_H = 116 * UI_SCALE;
#endif

#define PY_SSIZE_T_CLEAN
#undef slots
#include <Python.h>
#define slots Q_SLOTS

#define LINKAMP_PY_MODULE "linamp"

MainWindow::MainWindow(QWidget *parent)
     : QMainWindow{parent}
{
    // Initialize Python interpreter, required by BT and Spotify sources
    Py_Initialize();
    PyEval_SaveThread();

    // Setup playlist
    m_playlistModel = new PlaylistModel(this);
    m_playlist = m_playlistModel->playlist();
    connect(m_playlist, &QMediaPlaylist::mediaInserted, this, &MainWindow::schedulePlaylistSave);
    connect(m_playlist, &QMediaPlaylist::mediaRemoved, this, &MainWindow::schedulePlaylistSave);
    connect(m_playlist, &QMediaPlaylist::mediaChanged, this, &MainWindow::schedulePlaylistSave);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSavedPlaylist);
    loadSavedPlaylist();

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
    btSource = new AudioSourcePython(LINKAMP_PY_MODULE, "BTPlayer", this);
    cdSource = new AudioSourceCDNative(this);
    spotSource = new AudioSourcePython(LINKAMP_PY_MODULE, "SpotifyPlayer", this);
    coordinator->addSource(fileSource, "FILE", true);
    coordinator->addSource(btSource, "BT", false);
    coordinator->addSource(cdSource, "CD", false);
    coordinator->addSource(spotSource, "SPOT", false);


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
        connect(mainTitleBar, &TitleBar::menuClicked, this, &MainWindow::showMenu);
        connect(mainTitleBar, &TitleBar::shadeClicked, this, &MainWindow::toggleWindowShade);
        connect(mainTitleBar, &TitleBar::moveStarted, this, [this](bool detach) {
            beginAttachedWindowDrag(this, detach);
        });
        connect(mainTitleBar, &TitleBar::moveFinished, this, [this] {
            finishAttachedWindowDrag(this);
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

    // Keep the classic playlist editor as a separate compact top-level window.
    playlist = new ClassicPlaylistWindow(m_playlist, this);
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

    // The equalizer is also a separate classic-style window.
    m_equalizerWindow = new QDialog(this, Qt::Tool | Qt::FramelessWindowHint);
    m_equalizerWindow->installEventFilter(this);
    m_equalizerWindow->setWindowTitle(tr("Linkamp Equalizer"));
    m_equalizerWindow->setFixedSize(275, 116);
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

    QTimer::singleShot(50, this, [this] {
        if (playlist && !playlist->isVisible())
            showPlaylist();
    });
}

MainWindow::~MainWindow()
{
    saveSavedPlaylist();
    PyGILState_Ensure();
    Py_Finalize();
}

QString MainWindow::defaultPlaylistPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
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
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSavedPlaylist();
    QMainWindow::closeEvent(event);
}

void MainWindow::showPlayer()
{
    if (playlist)
        playlist->hide();
    if (m_equalizerWindow)
        m_equalizerWindow->hide();
    show();
    raise();
    activateWindow();
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
        playlist->move(frameGeometry().left(), frameGeometry().bottom() + 1);
        m_lastPlaylistPosition = playlist->pos();
        m_havePlaylistPosition = true;
        m_syncingAttachedWindows = false;
        m_playlistAttached = true;
    } else if (m_playlistAttached) {
        const int y = m_equalizerWindow && m_equalizerWindow->isVisible()
            ? m_equalizerWindow->frameGeometry().bottom() + 1
            : frameGeometry().bottom() + 1;
        m_syncingAttachedWindows = true;
        playlist->move(frameGeometry().left(), y);
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
    QAction *equalizerAction = menu.addAction(tr("Equalizer"));
    menu.addSeparator();
    QAction *fileSourceAction = menu.addAction(tr("File source"));
    QAction *cdSourceAction = menu.addAction(tr("CD source"));
    QAction *bluetoothSourceAction = menu.addAction(tr("Bluetooth source"));
    QAction *spotifySourceAction = menu.addAction(tr("Spotify source"));

    QAction *selected = menu.exec(QCursor::pos());
    if (selected == openFiles)
        open();
    else if (selected == addYouTube)
        promptForYouTubeUrl();
    else if (selected == playlistAction)
        showPlaylist();
    else if (selected == equalizerAction)
        showEqualizer();
    else if (selected == fileSourceAction)
        coordinator->setSource(0);
    else if (selected == bluetoothSourceAction)
        coordinator->setSource(1);
    else if (selected == cdSourceAction)
        coordinator->setSource(2);
    else if (selected == spotifySourceAction)
        coordinator->setSource(3);
}

void MainWindow::showEqualizer()
{
    if (!m_equalizerWindow)
        return;
    if (m_equalizerWindow->isVisible()) {
        m_equalizerWindow->hide();
        if (playlist && playlist->isVisible() && m_playlistAttached && m_equalizerAttached) {
            m_syncingAttachedWindows = true;
            playlist->move(frameGeometry().left(), frameGeometry().bottom() + 1);
            m_lastPlaylistPosition = playlist->pos();
            m_syncingAttachedWindows = false;
        }
        return;
    }
    if (!m_haveEqualizerPosition) {
        m_syncingAttachedWindows = true;
        m_equalizerWindow->move(frameGeometry().left(), frameGeometry().bottom() + 1);
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        m_haveEqualizerPosition = true;
        m_syncingAttachedWindows = false;
        m_equalizerAttached = true;
    } else if (m_equalizerAttached) {
        m_syncingAttachedWindows = true;
        m_equalizerWindow->move(frameGeometry().left(), frameGeometry().bottom() + 1);
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        if (playlist && playlist->isVisible() && m_playlistAttached) {
            playlist->move(frameGeometry().left(), m_equalizerWindow->frameGeometry().bottom() + 1);
            m_lastPlaylistPosition = playlist->pos();
        }
        m_syncingAttachedWindows = false;
    }
    m_equalizerWindow->show();
    m_equalizerWindow->raise();
    m_equalizerWindow->activateWindow();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    m_lastMainPosition = pos();
    m_haveMainPosition = true;
    if (m_equalizerWindow != nullptr && !m_haveEqualizerPosition) {
        m_syncingAttachedWindows = true;
        m_equalizerWindow->move(frameGeometry().left(), frameGeometry().bottom() + 1);
        m_lastEqualizerPosition = m_equalizerWindow->pos();
        m_haveEqualizerPosition = true;
        m_syncingAttachedWindows = false;
    }
    if (playlist != nullptr && !m_havePlaylistPosition) {
        const int y = (m_equalizerWindow != nullptr && m_equalizerWindow->isVisible())
            ? m_equalizerWindow->frameGeometry().bottom() + 1
            : frameGeometry().bottom() + 1;
        m_syncingAttachedWindows = true;
        playlist->move(frameGeometry().left(), y);
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *window = qobject_cast<QWidget *>(watched);
    if (window != nullptr && event->type() == QEvent::Move
        && (window == playlist || window == m_equalizerWindow)) {
        QPoint *lastPosition = window == playlist ? &m_lastPlaylistPosition : &m_lastEqualizerPosition;
        bool *hasPosition = window == playlist ? &m_havePlaylistPosition : &m_haveEqualizerPosition;
        const QPoint current = window->pos();
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
    m_lastMainPosition = pos();
    m_lastPlaylistPosition = playlist != nullptr ? playlist->pos() : QPoint();
    m_lastEqualizerPosition = m_equalizerWindow != nullptr ? m_equalizerWindow->pos() : QPoint();
    m_haveMainPosition = true;
    m_havePlaylistPosition = playlist != nullptr;
    m_haveEqualizerPosition = m_equalizerWindow != nullptr;
    m_syncingAttachedWindows = false;
}

void MainWindow::beginAttachedWindowDrag(QWidget *window, bool detach)
{
    if (window == this) {
        m_mainForceDetach = detach;
        if (detach) {
            m_playlistAttached = false;
            m_equalizerAttached = false;
        }
    } else if (window == playlist) {
        m_playlistForceDetach = detach;
        // Dragging an attached child window away from the group detaches it.
        // On release it will snap back if it was only nudged slightly.
        m_playlistAttached = false;
    } else if (window == m_equalizerWindow) {
        m_equalizerForceDetach = detach;
        m_equalizerAttached = false;
    }
}

void MainWindow::finishAttachedWindowDrag(QWidget *window)
{
    if (window == this) {
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
    }
}

bool MainWindow::snapWindowToMain(QWidget *window)
{
    if (window == nullptr)
        return false;

    const QRect mainRect = frameGeometry();
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
