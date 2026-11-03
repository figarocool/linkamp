#ifndef AUDIOSOURCEWSPECTRUMCAPTURE_H
#define AUDIOSOURCEWSPECTRUMCAPTURE_H

#include <QObject>
#include "audiosource.h"

#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
#include <QTimer>
#include <QtConcurrent>
#include <signal.h>
#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

struct PwData {
        struct pw_main_loop *loop;
        struct pw_stream *stream;

        struct spa_audio_info format;
        unsigned move:1;

        QMutex *sampleMutex;
        QByteArray *sample;
        QDataStream *sampleStream;
};
#endif

class AudioSourceWSpectrumCapture : public AudioSource
{
    Q_OBJECT
public:
    explicit AudioSourceWSpectrumCapture(QObject *parent = nullptr);
    ~AudioSourceWSpectrumCapture();

    void startSpectrum();
    void stopSpectrum();

private:
#ifdef LINKAMP_HAS_LINUX_INTEGRATIONS
    bool spectrumRunning = false;
    QTimer *dataEmitTimer = nullptr;
    void emitData();

    QAudioFormat spectrumDataFormat;

    struct PwData pwData;
    void pwLoop();

    QFuture<void> pwLoopThread;
#endif

signals:

};

#endif // AUDIOSOURCEWSPECTRUMCAPTURE_H
