#ifndef PLAYERVIEW_H
#define PLAYERVIEW_H

#include "controlbuttonswidget.h"
#include "mediaplayer.h"
#include "spectrumwidget.h"

#include <QMediaMetaData>
#include <QAudioOutput>
#include <QWidget>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QAbstractItemView;
class QLabel;
class QModelIndex;
class QPushButton;
class QComboBox;
class QSlider;
class QStatusBar;
QT_END_NAMESPACE

namespace Ui {
class PlayerView;
}

class PlayerView : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerView(QWidget *parent = nullptr, ControlButtonsWidget *ctlBtns = nullptr);
    ~PlayerView();

    void setSourceLabel(QString label);

public slots:
    void setDeferredSeekEnabled(bool enabled);
    void setPlaybackState(MediaPlayer::PlaybackState state);
    void setPosition(qint64 progress);
    void setSpectrumData(const QByteArray& data, QAudioFormat format);
    void setMetadata(QMediaMetaData metadata);
    void setDuration(qint64 duration);
    void setVolume(int volume);
    void setBalance(int balance);
    void setEqEnabled(bool enabled);
    void setPlEnabled(bool enabled);
    void setShuffleEnabled(bool enabled);
    void setRepeatEnabled(bool enabled);
    void setMessage(QString message, qint64 timeout);
    void setPersistentMessage(const QString &message);
    void clearMessage();

signals:
    void currentTrackTitleChanged(const QString &title);
    void volumeChanged(int volume);
    void balanceChanged(int balance);
    void volumeDragStarted();
    void volumeDragFinished(int volume);
    void balanceDragStarted();
    void balanceDragFinished(int balance);
    void positionChanged(qint64 progress);
    void playbackPositionChanged(qint64 progress);
    void eqClicked();
    void plClicked();
    void previousClicked();
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void nextClicked();
    void openClicked();
    void shuffleClicked();
    void repeatClicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::PlayerView *ui;
    void scale();
    void updatePositionSliderStyle();
    SpectrumWidget *spectrum = nullptr;
    QTimer *messageTimer = nullptr;
    ControlButtonsWidget *controlButtons = nullptr;

    QString m_trackInfo;
    QString m_statusInfo;
    qint64 m_duration = 0;
    qint64 m_currentPositionSeconds = 0;

    bool plEnabled = false;
    bool eqEnabled = false;
    bool shuffleEnabled = false;
    bool repeatEnabled = false;
    bool m_isUserSeeking = false;
    bool m_deferredSeekEnabled = false;
    bool m_monoPlayback = false;
    bool m_timeLeftMode = true;
    MediaPlayer::PlaybackState m_playbackState = MediaPlayer::StoppedState;

    void updateDurationInfo(qint64 currentInfo);
    void setChannelIndicators(bool mono = false, bool active = true);
    QLabel *m_monoIndicator = nullptr;
    QLabel *m_stereoIndicator = nullptr;
    void setTrackInfo(const QString &info);

private slots:
    void handleBalanceChanged();

};

#endif // PLAYERVIEW_H
