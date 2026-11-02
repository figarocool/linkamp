#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedLayout>
#include <QProcess>
#include <QSet>
#include "audiosourcecdnative.h"
#include "audiosourcecoordinator.h"
#include "audiosourcefile.h"
#include "audiosourcepython.h"
#include "controlbuttonswidget.h"
#include "equalizerview.h"
#include "playerview.h"
#include "classicplaylistwindow.h"
#include "systemequalizer.h"
#include "qmediaplaylist.h"
#include "playlistmodel.h"

class QDialog;
class QMoveEvent;
class QShowEvent;
class QCloseEvent;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QStackedLayout *viewStack;

    PlayerView *player;
    ControlButtonsWidget *controlButtons;
    ClassicPlaylistWindow *playlist;
    EqualizerView *equalizerView;
    SystemEqualizer *equalizer;
    AudioSourceCoordinator *coordinator;
    AudioSourceFile *fileSource;
    AudioSourcePython *btSource;
    AudioSourceCDNative *cdSource;
    AudioSourcePython *spotSource;

public slots:
    void showPlayer();
    void showPlaylist();
    void showMenu();
    void showEqualizer();
    void toggleWindowShade();
    void showShutdownModal();
    void open();
    void addFolderToPlaylist();
    void promptForYouTubeUrl();
    void addYouTubeUrl(const QString &url);

protected:
    void moveEvent(QMoveEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void loadSavedPlaylist();
    void schedulePlaylistSave();
    void saveSavedPlaylist();
    static QString defaultPlaylistPath();
    QTimer *m_playlistSaveTimer = nullptr;
    bool m_isLoadingPlaylist = false;

private:
    QMediaPlaylist *m_playlist = nullptr;
    PlaylistModel *m_playlistModel = nullptr;
    QSet<QString> m_pendingYouTubeIds;
    QDialog *m_equalizerWindow = nullptr;
    QWidget *m_playerBodyContainer = nullptr;
    bool m_windowShaded = false;
    int m_restoreWindowHeight = 116;
    bool m_equalizerWindowShaded = false;
    int m_restoreEqualizerWindowHeight = 116;
    bool m_syncingAttachedWindows = false;
    bool m_haveMainPosition = false;
    bool m_havePlaylistPosition = false;
    bool m_haveEqualizerPosition = false;
    bool m_playlistAttached = true;
    bool m_equalizerAttached = true;
    bool m_mainForceDetach = false;
    bool m_playlistForceDetach = false;
    bool m_equalizerForceDetach = false;
    QPoint m_lastMainPosition;
    QPoint m_lastPlaylistPosition;
    QPoint m_lastEqualizerPosition;
    QProcess *shutdownProcess = nullptr;
    void shutdown();
    void moveAttachedWindows(QWidget *source, const QPoint &delta);
    void beginAttachedWindowDrag(QWidget *window, bool detach);
    void finishAttachedWindowDrag(QWidget *window);
    void toggleEqualizerWindowShade();
    bool snapWindowToMain(QWidget *window);

};

#endif // MAINWINDOW_H
