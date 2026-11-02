#include <QTime>
#include <QFileDialog>
#include <QProcess>
#include <QStandardPaths>

#include "audiosourcefile.h"
#include "util.h"

//#define DISPLAY_BUFFERING


AudioSourceFile::AudioSourceFile(QObject *parent, PlaylistModel *playlistModel)
    : AudioSourceWSpectrumCapture{parent}
{
    m_player = new MediaPlayer(this);

    connect(m_player, &MediaPlayer::durationChanged, this, [this](qint64 duration) {
        const QUrl currentMedia = m_playlist->currentQueueMedia();
        if (isYouTubeUrl(currentMedia)) {
            const qint64 playlistDuration = m_playlist->queueMediaMetadata(
                m_playlist->currentQueueIndex()).value(QMediaMetaData::Duration).toLongLong();
            if (playlistDuration > 0)
                duration = playlistDuration;
        }
        emit durationChanged(duration);
    });
    connect(m_player, &MediaPlayer::positionChanged, this, &AudioSourceFile::positionChanged);
    connect(m_player, QOverload<>::of(&MediaPlayer::metaDataChanged), this,
            &AudioSourceFile::handleMetaDataChanged);
    connect(m_player, &MediaPlayer::mediaStatusChanged, this, &AudioSourceFile::handleMediaStatusChanged);
    connect(m_player, &MediaPlayer::bufferProgressChanged, this, &AudioSourceFile::handleBufferingProgress);
    connect(m_player, &MediaPlayer::errorChanged, this, &AudioSourceFile::handleMediaError);

    m_playlistModel = playlistModel;
    m_playlist = m_playlistModel->playlist();

    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this,
            &AudioSourceFile::handlePlaylistPositionChanged);
    connect(m_playlist, &QMediaPlaylist::mediaAboutToBeRemoved, this,
            &AudioSourceFile::handlePlaylistMediaRemoved);

    connect(m_player, &MediaPlayer::playbackStateChanged, this, &AudioSourceFile::playbackStateChanged);

}

void AudioSourceFile::activate()
{
    emit playbackStateChanged(m_player->playbackState());
    emit positionChanged(m_player->position());
    const QMediaMetaData metadata = currentMetaData();
    emit metadataChanged(metadata);
    const qint64 duration = metadata.value(QMediaMetaData::Duration).toLongLong();
    emit durationChanged(duration > 0 ? duration : m_player->duration());
    emit eqEnabledChanged(false);
    emit plEnabledChanged(true);
    emit shuffleEnabledChanged(shuffleEnabled);
    emit repeatEnabledChanged(repeatEnabled);

    if (m_player->playbackState() == MediaPlayer::PlaybackState::PlayingState) {
        startSpectrum();
    } else {
        stopSpectrum();
    }
}

void AudioSourceFile::deactivate()
{
    // Stop everything
    shouldBePlaying = false;
    if (m_youtubeResolveProcess) {
        QProcess *process = m_youtubeResolveProcess;
        m_youtubeResolveProcess = nullptr;
        process->kill();
        process->deleteLater();
    }
    stopSpectrum();
    m_player->stop();
}

void AudioSourceFile::handlePl()
{
    emit showPlaylistRequested();
    emit plEnabledChanged(true);
}

void AudioSourceFile::handlePrevious()
{
    // The previous-track control always navigates the playlist. Seeking is
    // handled by the position slider, independently of the current position.
    m_playlist->previous();
}

void AudioSourceFile::handlePlay()
{
    shouldBePlaying = true;
    if (m_playlist->currentIndex() < 0 && m_playlist->mediaCount() > 0) {
        m_playlist->setCurrentIndex(0);
        return;
    }
    const QUrl currentMedia = m_playlist->currentQueueMedia();
    if (isYouTubeUrl(currentMedia)) {
        if (m_resolvedYouTubeMedia == currentMedia) {
            m_player->play();
            startSpectrum();
            return;
        }
        handlePlaylistPositionChanged(m_playlist->currentIndex());
        return;
    }
    m_player->play();
    startSpectrum();
}

void AudioSourceFile::handlePause()
{
    shouldBePlaying = false;
    m_player->pause();
    stopSpectrum();
}

void AudioSourceFile::handleStop()
{
    shouldBePlaying = false;
    if (m_youtubeResolveProcess) {
        QProcess *process = m_youtubeResolveProcess;
        m_youtubeResolveProcess = nullptr;
        process->kill();
        process->deleteLater();
    }
    m_player->stop();
    stopSpectrum();
}

void AudioSourceFile::handleNext()
{
    // If is last item in playlist:
    // If repeat enabled, go to first
    // If not, do nothing
    // That's handled by QMediaPlaylist
    m_playlist->next();
}

void AudioSourceFile::handleOpen()
{
    emit openFilesRequested();
}

void AudioSourceFile::handleShuffle()
{
    shuffleEnabled = !shuffleEnabled;
    m_playlist->setShuffle(shuffleEnabled);
    emit shuffleEnabledChanged(shuffleEnabled);
}

void AudioSourceFile::handleRepeat()
{
    repeatEnabled = !repeatEnabled;
    QMediaPlaylist::PlaybackMode mode = QMediaPlaylist::PlaybackMode::Sequential;
    if(repeatEnabled) mode = QMediaPlaylist::PlaybackMode::Loop;
    m_playlist->setPlaybackMode(mode);
    emit repeatEnabledChanged(repeatEnabled);
}

void AudioSourceFile::handleSeek(int mseconds)
{
    m_player->setPosition(mseconds);

}

void AudioSourceFile::handleMetaDataChanged()
{
    const QMediaMetaData metadata = currentMetaData();
    emit metadataChanged(metadata);
    const qint64 duration = metadata.value(QMediaMetaData::Duration).toLongLong();
    if (duration > 0)
        emit durationChanged(duration);
}

void AudioSourceFile::handleMediaStatusChanged(MediaPlayer::MediaStatus status)
{
    // handle status message
    switch (status) {
    case MediaPlayer::NoMedia:
    case MediaPlayer::LoadedMedia:
        setStatusInfo(QString());
        break;
    case MediaPlayer::LoadingMedia:
        setStatusInfo(tr("Loading..."));
        break;
    case MediaPlayer::BufferingMedia:
    case MediaPlayer::BufferedMedia:
        #ifdef DISPLAY_BUFFERING
        setStatusInfo(tr("Buffering %1%").arg(qRound(m_player->bufferProgress())));
        #endif
        break;
    case MediaPlayer::StalledMedia:
        setStatusInfo(tr("Stalled %1%").arg(qRound(m_player->bufferProgress())));
        break;
    case MediaPlayer::EndOfMedia:
        handleNext();
        break;
    case MediaPlayer::InvalidMedia:
        handleMediaError();
        break;
    }
}

void AudioSourceFile::handleBufferingProgress(float progress)
{

    if (m_player->mediaStatus() == MediaPlayer::StalledMedia) {
        setStatusInfo(tr("Stalled %1%").arg(qRound(progress)));
    }
    else {
        #ifdef DISPLAY_BUFFERING
        setStatusInfo(tr("Buffering %1%").arg(qRound(progress)));
        #endif
    }
}

void AudioSourceFile::handleMediaError()
{
    if (m_player->error() == MediaPlayer::NoError)
        return;
    setStatusInfo(m_player->errorString());
    qDebug() << m_player->errorString();
}

void AudioSourceFile::setStatusInfo(const QString &info)
{
    m_statusInfo = info;

    if (!m_statusInfo.isEmpty())
        emit messageSet(QString("%1").arg(m_statusInfo), 1000);
    else
        emit messageClear();
}

void AudioSourceFile::handlePlaylistPositionChanged(int)
{
    m_resolvedYouTubeMedia = QUrl();
    if (m_youtubeResolveProcess) {
        QProcess *previousProcess = m_youtubeResolveProcess;
        m_youtubeResolveProcess = nullptr;
        previousProcess->kill();
        previousProcess->deleteLater();
    }

    const QUrl media = m_playlist->currentQueueMedia();
    if (isYouTubeUrl(media)) {
        m_player->stop();
        m_player->clearSource();
        stopSpectrum();

        const QMediaMetaData metadata = currentMetaData();
        emit metadataChanged(metadata);
        emit durationChanged(metadata.value(QMediaMetaData::Duration).toLongLong());

        const QString ytDlp = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
        if (ytDlp.isEmpty()) {
            setStatusInfo(tr("yt-dlp non trovato"));
            return;
        }

        setStatusInfo(tr("Connessione a YouTube…"));
        auto *process = new QProcess(this);
        m_youtubeResolveProcess = process;
        connect(process, &QProcess::finished, this,
                [this, process, media](int exitCode, QProcess::ExitStatus) {
            const QString output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
            const QString errorOutput = QString::fromUtf8(process->readAllStandardError()).trimmed();
            if (m_youtubeResolveProcess != process) {
                process->deleteLater();
                return;
            }
            m_youtubeResolveProcess = nullptr;
            process->deleteLater();

            if (m_playlist->currentQueueMedia() != media)
                return;
            if (exitCode != 0) {
                setStatusInfo(errorOutput.isEmpty() ? tr("Impossibile aprire il link YouTube")
                                                    : errorOutput.left(180));
                return;
            }

            QString streamUrl;
            const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QString candidate = line.trimmed();
                if (candidate.startsWith(QLatin1String("https://"))
                    || candidate.startsWith(QLatin1String("http://"))) {
                    streamUrl = candidate;
                }
            }
            const QUrl resolvedUrl(streamUrl);
            if (streamUrl.isEmpty() || !resolvedUrl.isValid()) {
                setStatusInfo(tr("YouTube non ha restituito uno stream audio"));
                return;
            }

            setStatusInfo(QString());
            m_resolvedYouTubeMedia = media;
            m_player->setSource(resolvedUrl);
            if (shouldBePlaying) {
                m_player->play();
                startSpectrum();
            }
        });
        connect(process, &QProcess::errorOccurred, this,
                [this, process](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart || m_youtubeResolveProcess != process)
                return;
            m_youtubeResolveProcess = nullptr;
            setStatusInfo(tr("Impossibile avviare yt-dlp"));
            process->deleteLater();
        });
        process->start(ytDlp, {
            QStringLiteral("--no-playlist"), QStringLiteral("--no-warnings"),
            QStringLiteral("--quiet"), QStringLiteral("-f"),
            QStringLiteral("bestaudio[ext=m4a]/bestaudio"), QStringLiteral("--get-url"),
            QStringLiteral("--"), media.toString(QUrl::FullyEncoded)
        });
        return;
    }

    m_player->setSource(media);

    if (shouldBePlaying) {
        m_player->play();
        startSpectrum();
    } else {
        stopSpectrum();
    }
}

void AudioSourceFile::handlePlaylistMediaRemoved(int from, int to)
{
    // Stop only if currently playing file was removed
    int playingIndex = m_playlist->currentIndex();
    if(playingIndex >= from && playingIndex <= to) {
        shouldBePlaying = false;
        m_player->stop();
        m_player->clearSource();
        stopSpectrum();
    }
}

void AudioSourceFile::jump(const QModelIndex &index)
{
    if (index.isValid()) {
        shouldBePlaying = true;
        const int previousIndex = m_playlist->currentIndex();
        m_playlist->setCurrentIndex(index.row());
        if (previousIndex == index.row())
            handlePlaylistPositionChanged(index.row());
    }
}

void AudioSourceFile::addToPlaylist(const QList<QUrl> &urls)
{
    const int previousMediaCount = m_playlist->mediaCount();
    for (auto &url : urls) {
        if (isPlaylist(url))
            m_playlist->load(url);
        else
            m_playlist->addMedia(url);
    }
    if (m_playlist->mediaCount() > previousMediaCount) {
        // Start playing only if not already playing
        if(m_player->playbackState() == MediaPlayer::PlaybackState::StoppedState) {
            auto index = m_playlistModel->index(previousMediaCount, 0);
            jump(index);
        }
    }
}


void AudioSourceFile::addYouTubeToPlaylist(const QUrl &url, const QMediaMetaData &metadata)
{
    if (!isYouTubeUrl(url))
        return;

    const QString videoId = youtubeVideoId(url);
    for (int index = 0; index < m_playlist->mediaCount(); ++index) {
        const QUrl existingUrl = m_playlist->media(index);
        if ((!videoId.isEmpty() && youtubeVideoId(existingUrl) == videoId)
            || (videoId.isEmpty() && existingUrl == url)) {
            return;
        }
    }
    m_playlist->addMedia(url, metadata);
}

QMediaMetaData AudioSourceFile::currentMetaData() const
{
    QMediaMetaData metadata = m_player->metaData();
    const int queueIndex = m_playlist->currentQueueIndex();
    const QUrl media = m_playlist->queueMedia(queueIndex);
    if (queueIndex >= 0)
        metadata.insert(QMediaMetaData::TrackNumber, queueIndex + 1);
    if (!isYouTubeUrl(media))
        return metadata;

    const QMediaMetaData playlistMetadata = m_playlist->queueMediaMetadata(queueIndex);
    const QString title = playlistMetadata.value(QMediaMetaData::Title).toString();
    const QString artist = playlistMetadata.value(QMediaMetaData::AlbumArtist).toString();
    const qint64 duration = playlistMetadata.value(QMediaMetaData::Duration).toLongLong();
    if (!title.isEmpty())
        metadata.insert(QMediaMetaData::Title, title);
    if (!artist.isEmpty())
        metadata.insert(QMediaMetaData::AlbumArtist, artist);
    if (duration > 0)
        metadata.insert(QMediaMetaData::Duration, duration);
    metadata.insert(QMediaMetaData::Url, media);
    return metadata;
}
