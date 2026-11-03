#include "systemequalizer.h"

#include <algorithm>

SystemEqualizer::SystemEqualizer(QObject *parent)
    : QObject(parent)
{
    loadSettings();
}

SystemEqualizer::~SystemEqualizer() = default;

bool SystemEqualizer::isEnabled() const { return false; }
double SystemEqualizer::preampDb() const { return m_preampDb; }
double SystemEqualizer::bandDb(int index) const
{
    return index >= 0 && index < 10 ? m_bandDb[index] : 0.0;
}
QString SystemEqualizer::hardwareSink() const { return {}; }

void SystemEqualizer::setEnabled(bool enabled)
{
    Q_UNUSED(enabled);
    emit enabledChanged(false);
}

void SystemEqualizer::setPreampDb(double db)
{
    m_preampDb = db;
    savePreamp();
}

void SystemEqualizer::setBandDb(int index, double db)
{
    if (index < 0 || index >= 10)
        return;
    m_bandDb[index] = db;
    saveBand(index);
}

void SystemEqualizer::flush() {}
void SystemEqualizer::setHardwareVolume(int volume, int balance)
{
    Q_UNUSED(volume);
    Q_UNUSED(balance);
}
void SystemEqualizer::applyPending() {}

void SystemEqualizer::loadSettings()
{
    m_settings.beginGroup(QStringLiteral("equalizer"));
    m_preampDb = m_settings.value(QStringLiteral("preamp"), 0.0).toDouble();
    for (int i = 0; i < 10; ++i)
        m_bandDb[i] = m_settings.value(QStringLiteral("band%1").arg(i), 0.0).toDouble();
    m_settings.endGroup();
}

void SystemEqualizer::saveEnabled() {}
void SystemEqualizer::savePreamp()
{
    m_settings.setValue(QStringLiteral("equalizer/preamp"), m_preampDb);
}
void SystemEqualizer::saveBand(int index)
{
    m_settings.setValue(QStringLiteral("equalizer/band%1").arg(index), m_bandDb[index]);
}
void SystemEqualizer::queueParam(const QString &, double) {}
void SystemEqualizer::applyAll() {}
void SystemEqualizer::correctInitialSinkVolume() {}
bool SystemEqualizer::startPipeWire() { return false; }
