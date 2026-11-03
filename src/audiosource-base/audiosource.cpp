#include "audiosource.h"

AudioSource::AudioSource(QObject *parent)
    : QObject{parent}
{

}

void AudioSource::setOutputVolume(int volume)
{
    Q_UNUSED(volume);
}
