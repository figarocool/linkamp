#ifndef VIDEOWINDOW_H
#define VIDEOWINDOW_H

#include <QDialog>
#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVideoFrame>
#include <QVideoSink>

#include "mediaplayer.h"

class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QHideEvent;

class VideoWindow : public QDialog
{
    Q_OBJECT

public:
    explicit VideoWindow(QWidget *parent = nullptr);

    QVideoSink *videoSink() const { return m_videoSink; }
    void refreshSkin();
    bool isFullScreenMode() const { return m_fullScreen; }

public slots:
    void setPlaybackState(MediaPlayer::PlaybackState state);
    void setCurrentTrackTitle(const QString &title);
    void toggleFullScreenMode();

signals:
    void moveStarted(bool detach);
    void moveFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void handleVideoFrame(const QVideoFrame &frame);
    void showContextMenu(const QPoint &globalPos);
    QRect canvasRect() const;
    QRect closeButtonRect() const;

    QVideoSink *m_videoSink = nullptr;
    QImage m_currentFrame;
    QString m_trackTitle;
    MediaPlayer::PlaybackState m_playbackState = MediaPlayer::StoppedState;

    QElapsedTimer m_clock;
    QPoint m_dragOffset;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
    QRect m_windowedGeometry;
    qint64 m_lastCanvasClickMs = -1000;
    qint64 m_lastFullScreenToggleMs = -1000;
    bool m_dragging = false;
    bool m_resizing = false;
    bool m_closePressed = false;
    bool m_fullScreen = false;
};

#endif // VIDEOWINDOW_H
