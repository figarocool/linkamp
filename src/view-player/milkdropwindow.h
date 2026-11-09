#ifndef MILKDROPWINDOW_H
#define MILKDROPWINDOW_H

#include <QAudioFormat>
#include <QByteArray>
#include <QDialog>
#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QSize>
#include <QTimer>
#include "mediaplayer.h"

class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QHideEvent;

class MilkdropWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MilkdropWindow(QWidget *parent = nullptr);

    void refreshSkin();
    bool isFullScreenMode() const { return m_fullScreen; }

public slots:
    void setAudioData(const QByteArray &data, QAudioFormat format);
    void setPlaybackState(MediaPlayer::PlaybackState state);
    void nextPreset();
    void previousPreset();
    void randomPreset();
    void selectPreset(int index);
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
    static constexpr int kBufW = 256;
    static constexpr int kBufH = 144;
    static constexpr int kGridX = 32;
    static constexpr int kGridY = 18;
    static constexpr int kSamples = 512;

    struct WarpParams {
        float zoom = 1.02f;
        float rot = 0.0f;
        float warp = 0.0f;
        float cx = 0.5f;
        float cy = 0.5f;
        float dx = 0.0f;
        float dy = 0.0f;
        float decayR = 0.96f;
        float decayG = 0.96f;
        float decayB = 0.96f;
        float colorShift = 0.0f;
        int waveMode = 0;
        int symmetry = 0;
    };

    void stepSimulation();
    void renderWaveforms(QPainter &bufPainter, float timeSec);
    void triggerPresetOverlay();
    void showContextMenu(const QPoint &globalPos);
    QRect canvasRect() const;
    QRect closeButtonRect() const;
    WarpParams evaluatePresetParams(int presetIndex, float timeSec) const;
    void evaluateVertexWarp(int presetIndex, float x, float y, float rad, float ang,
                            float timeSec, const WarpParams &p, float *outU, float *outV) const;

    QTimer m_frameTimer;
    QElapsedTimer m_clock;
    QImage m_frontBuffer;
    QImage m_backBuffer;

    float m_pcmLeft[kSamples] = {};
    float m_pcmRight[kSamples] = {};
    float m_spectrum[64] = {};

    float m_bass = 0.0f;
    float m_mid = 0.0f;
    float m_treb = 0.0f;
    float m_bassAtt = 0.1f;
    float m_midAtt = 0.1f;
    float m_trebAtt = 0.1f;
    float m_beatPulse = 0.0f;
    qint64 m_lastBeatMs = 0;
    qint64 m_lastAudioMs = 0;
    qint64 m_presetSwitchMs = 0;
    qint64 m_overlayUntilMs = 0;

    int m_currentPreset = 0;
    int m_previousPreset = 0;
    float m_blendProgress = 1.0f;
    bool m_autoCycle = true;
    MediaPlayer::PlaybackState m_playbackState = MediaPlayer::StoppedState;

    QPoint m_dragOffset;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
    QRect m_windowedGeometry;
    qint64 m_lastCanvasClickMs = -1000;
    qint64 m_lastFullScreenToggleMs = -1000;
    bool m_fullScreen = false;
    bool m_dragging = false;
    bool m_resizing = false;
    bool m_closePressed = false;
};

#endif // MILKDROPWINDOW_H
