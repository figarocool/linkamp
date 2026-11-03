#include "equalizerview.h"
#include "winampskin.h"
#include "equalizergraph.h"
#include "equalizerpresetview.h"
#include "equalizerbands.h"
#include "equalizerpreset.h"
#include "systemequalizer.h"
#include <QCheckBox>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>

namespace {

class EqFader : public QSlider
{
public:
    using QSlider::QSlider;

    QRect handleRectFor(int value) const
    {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        opt.sliderPosition = value;
        opt.sliderValue = value;
        return style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        const QPixmap sprite = QPixmap::fromImage(
            WinampSkin::instance().sheet(QStringLiteral("eqmain")));
        if (sprite.isNull())
            return;
        QPainter painter(this);
        const QRect handle = handleRectFor(value());
        painter.drawPixmap(handle, sprite, QRect(0, isSliderDown() ? 176 : 164, 11, 11));
    }
};

// Slider units are 0.1 dB, so 5 is ±0.5 dB.
constexpr int kZeroSnap = 5;

bool snapSliderToZero(QSlider *slider)
{
    if (!slider->isSliderDown()) {
        return false;
    }
    const int value = slider->value();
    if (value >= -kZeroSnap && value <= kZeroSnap && value != 0) {
        slider->setValue(0);
        return true;
    }
    return false;
}

}

EqualizerView::EqualizerView(SystemEqualizer *equalizer, QWidget *parent)
    : QWidget(parent)
    , m_equalizer(equalizer)
{
    setObjectName(QStringLiteral("EqualizerView"));
    setFixedSize(275, 102);
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);

    const QString invisibleCheckBox = QStringLiteral(
        "QCheckBox { background: transparent; spacing: 0px; }"
        "QCheckBox::indicator { width: 0px; height: 0px; }");
    m_onButton = new QCheckBox(this);
    m_onButton->setGeometry(14, 4, 25, 12);
    m_onButton->setText(QString());
    m_onButton->setCursor(Qt::PointingHandCursor);
    m_onButton->setStyleSheet(invisibleCheckBox);
    m_onButton->setChecked(m_equalizer->isEnabled());

    m_autoButton = new QCheckBox(this);
    m_autoButton->setGeometry(39, 4, 33, 12);
    m_autoButton->setText(QString());
    m_autoButton->setCursor(Qt::PointingHandCursor);
    m_autoButton->setStyleSheet(invisibleCheckBox);

    m_valueHideTimer = new QTimer(this);
    m_valueHideTimer->setSingleShot(true);
    m_valueHideTimer->setInterval(1500);
    m_valueLabel = new QLabel(this);
    m_valueLabel->hide();
    connect(m_valueHideTimer, &QTimer::timeout, m_valueLabel, &QLabel::clear);

    m_graph = new EqualizerGraph(this);
    double bands[EqBands::Count];
    for (int i = 0; i < EqBands::Count; ++i) {
        bands[i] = m_equalizer->bandDb(i);
    }
    m_graph->setBands(bands, EqBands::Count);
    m_graph->setPreampDb(m_equalizer->preampDb());
    m_graph->setFixedSize(113, 19);
    m_graph->move(86, 3);

    auto *presetsButton = new QPushButton(this);
    presetsButton->setCursor(Qt::PointingHandCursor);
    presetsButton->setGeometry(217, 4, 44, 12);
    presetsButton->setStyleSheet(QStringLiteral("QPushButton { border: 0; background: transparent; }") );

    m_preampSlider = makeSlider(m_equalizer->preampDb());
    m_preampSlider->setGeometry(21, 24, 14, 63);

    for (int i = 0; i < EqBands::Count; ++i) {
        m_bandSliders[i] = makeSlider(m_equalizer->bandDb(i));
        m_bandSliders[i]->setGeometry(78 + 18 * i, 24, 14, 63);
    }

    connect(m_onButton, &QCheckBox::toggled, m_equalizer, &SystemEqualizer::setEnabled);
    connect(m_onButton, &QCheckBox::toggled, this, [this] { update(); });
    connect(m_autoButton, &QCheckBox::toggled, this, [this] { update(); });
    connect(m_equalizer, &SystemEqualizer::enabledChanged, this, [this](bool enabled) {
        if (m_onButton->isChecked() != enabled) {
            m_onButton->blockSignals(true);
            m_onButton->setChecked(enabled);
            m_onButton->blockSignals(false);
        }
        update();
    });

    connect(m_preampSlider, &QSlider::valueChanged, this, [this](int value) {
        if (snapSliderToZero(m_preampSlider)) {
            return;
        }
        const double db = EqBands::sliderToDb(value);
        m_equalizer->setPreampDb(db);
        m_graph->setPreampDb(db);
        showValue("PRE", db);
        update();
        if (!m_applyingPreset && m_presetView != nullptr) {
            m_presetView->onManualChange();
        }
    });
    connect(m_preampSlider, &QSlider::sliderReleased, m_equalizer, &SystemEqualizer::flush);

    for (int i = 0; i < EqBands::Count; ++i) {
        connect(m_bandSliders[i], &QSlider::valueChanged, this, [this, i](int value) {
            if (snapSliderToZero(m_bandSliders[i])) {
                return;
            }
            const double db = EqBands::sliderToDb(value);
            m_equalizer->setBandDb(i, db);
            m_graph->setBandDb(i, db);
            showValue(EqBands::label(i), db);
            update();
            if (!m_applyingPreset && m_presetView != nullptr) {
                m_presetView->onManualChange();
            }
        });
    connect(m_bandSliders[i], &QSlider::sliderReleased, m_equalizer, &SystemEqualizer::flush);
    }

    m_presetView = new EqualizerPresetView(this);
    m_presetView->hide();
    connect(presetsButton, &QPushButton::clicked, this, [this] {
        m_presetView->setGeometry(rect());
        m_presetView->show();
        m_presetView->raise();
    });
    connect(m_presetView, &EqualizerPresetView::presetChosen, this, &EqualizerView::applyPreset);
    connect(m_presetView, &EqualizerPresetView::saveClicked, this, &EqualizerView::saveCurrentPreset);
    connect(m_presetView, &EqualizerPresetView::updateClicked, this, &EqualizerView::updateCurrentPreset);
}

void EqualizerView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    const QPixmap sprite = QPixmap::fromImage(
        WinampSkin::instance().sheet(QStringLiteral("eqmain")));
    if (sprite.isNull())
        return;

    QPainter painter(this);
    painter.drawPixmap(rect(), sprite, QRect(0, 14, 275, 102));
    painter.drawPixmap(QRect(86, 3, 113, 19), sprite, QRect(0, 294, 113, 19));

    auto drawRail = [&](QSlider *slider, int x) {
        const int pos = std::clamp((slider->value() - EqBands::SliderMin) * 63
                                       / (EqBands::SliderMax - EqBands::SliderMin),
                                   0, 63);
        const int tile = 27 - (pos * 28) / 64;
        const QRect source = tile < 14
            ? QRect(13 + tile * 15, 164, 14, 63)
            : QRect(13 + (tile - 14) * 15, 229, 14, 63);
        painter.drawPixmap(QRect(x, 24, 14, 63), sprite, source);
    };
    drawRail(m_preampSlider, 21);
    for (int i = 0; i < EqBands::Count; ++i)
        drawRail(m_bandSliders[i], 78 + 18 * i);

    const int onSource = m_onButton->isChecked() ? 69 : 10;
    const int autoSource = m_autoButton->isChecked() ? 94 : 35;
    painter.drawPixmap(QRect(14, 4, 25, 12), sprite, QRect(onSource, 119, 25, 12));
    painter.drawPixmap(QRect(39, 4, 33, 12), sprite, QRect(autoSource, 119, 33, 12));
    painter.drawPixmap(QRect(217, 4, 44, 12), sprite,
                       QRect(224, m_presetView && m_presetView->isVisible() ? 176 : 164, 44, 12));
}

void EqualizerView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_presetView != nullptr) {
        m_presetView->setGeometry(rect());
    }
}

void EqualizerView::applyPreset(const EqPreset &preset)
{
    m_applyingPreset = true;
    m_equalizer->setEnabled(true);
    m_preampSlider->setValue(EqBands::dbToSlider(preset.preampDb));
    for (int i = 0; i < EqBands::Count; ++i) {
        m_bandSliders[i]->setValue(EqBands::dbToSlider(preset.bandDb[i]));
    }
    m_equalizer->flush();
    m_applyingPreset = false;
}

void EqualizerView::saveCurrentPreset()
{
    m_presetView->saveFrom(snapshotGains());
}

void EqualizerView::updateCurrentPreset()
{
    m_presetView->updateSelected(snapshotGains());
}

EqPreset EqualizerView::snapshotGains() const
{
    EqPreset preset;
    preset.preampDb = m_equalizer->preampDb();
    for (int i = 0; i < EqBands::Count; ++i) {
        preset.bandDb[i] = m_equalizer->bandDb(i);
    }
    return preset;
}

void EqualizerView::showValue(const QString &name, double db)
{
    const QString sign = db > 0.05 ? "+" : "";
    m_valueLabel->setText(QString("%1  %2%3 dB").arg(name, sign).arg(db, 0, 'f', 1));
    m_valueHideTimer->start();
}

QSlider *EqualizerView::makeSlider(double db)
{
    auto *slider = new EqFader(Qt::Vertical, this);
    slider->setRange(EqBands::SliderMin, EqBands::SliderMax);
    slider->setSingleStep(1);
    slider->setPageStep(10);
    slider->setTracking(true);
    slider->setFocusPolicy(Qt::NoFocus);
    slider->setStyleSheet(QStringLiteral(
        "QSlider { background: transparent; }"
        "QSlider::groove:vertical { width: 14px; background: transparent; border: 0; }"
        "QSlider::handle:vertical { width: 11px; height: 11px; background: transparent; border: 0; }"
        "QSlider::sub-page:vertical, QSlider::add-page:vertical { background: transparent; border: 0; }"));
    slider->setValue(EqBands::dbToSlider(db));
    slider->setFixedSize(14, 63);
    return slider;
}
