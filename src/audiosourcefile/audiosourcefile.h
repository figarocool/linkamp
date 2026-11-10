#ifndef AUDIOSOURCEFILE_H
#define AUDIOSOURCEFILE_H

#include <QObject>

#include "audiosourcewspectrumcapture.h"
#include "qmediaplaylist.h"
#include "playlistmodel.h"
#include "mediaplayer.h"

class QProcess;
class QTimer;
class QVideoSink;

class AudioSourceFile : public AudioSourceWSpectrumCapture
{
    Q_OBJECT
public:
    explicit AudioSourceFile(QObject *parent = nullptr, PlaylistModel *playlistModel = nullptr);
    void setOutputVolume(int volume) override;
    void setVideoSink(QVideoSink *sink);

signals:
    void showPlaylistRequested();
    void openFilesRequested();

public slots:
    void activate();
    void deactivate();
    void handlePl();
    void handlePrevious();
    void handlePlay();
    void handlePause();
    void handleStop();
    void handleNext();
    void handleOpen();
    void handleShuffle();
    void handleRepeat();
    void handleSeek(int mseconds);

    void jump(const QModelIndex &index);

    void addToPlaylist(const QList<QUrl> &urls);
    void addYouTubeToPlaylist(const QUrl &url, const QMediaMetaData &metadata);

private slots:
    void handleMetaDataChanged();
    void handleMediaStatusChanged(MediaPlayer::MediaStatus status);
    void handleBufferingProgress(float progress);
    void handleMediaError();
    void handlePlaylistPositionChanged(int);
    void handlePlaylistMediaRemoved(int, int);


private:
    MediaPlayer *m_player = nullptr;
    QMediaPlaylist *m_playlist = nullptr;
    PlaylistModel *m_playlistModel = nullptr;

    QString m_statusInfo;
    QUrl m_resolvedYouTubeMedia;
    QUrl m_streamTrack;
    int m_streamSampleRateHz = 0;
    int m_streamBitrateKbps = 0;
    int m_streamRetryAttempt = 0;
    qint64 m_streamResumePosition = 0;
    bool m_streamRetryPending = false;
    bool m_streamSeekPending = false;

    bool shuffleEnabled = false;
    bool repeatEnabled = false;
    bool shouldBePlaying = false;
    QProcess *m_youtubeResolveProcess = nullptr;
    QTimer *m_streamRetryTimer = nullptr;

    void setStatusInfo(const QString &info);
    void scheduleYouTubeRetry();
    QMediaMetaData currentMetaData() const;

};

#endif // AUDIOSOURCEFILE_H
