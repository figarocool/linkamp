#include "linkampslider.h"
#include "winampskin.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QStyle>
#include <QStyleOptionSlider>

#include <algorithm>
#include <cstdlib>

LinkampSlider::LinkampSlider(QWidget* parent) :
    QSlider(parent),
    gradient(QSize(100,100), QImage::Format_RGB32)
{
}

void LinkampSlider::paintEvent(QPaintEvent *event)
{
    WinampSkin &skin = WinampSkin::instance();
    const bool isVolume = objectName() == QLatin1String("volumeSlider");
    const bool isBalance = objectName() == QLatin1String("balanceSlider");
    if (!isVolume && !isBalance) {
        QSlider::paintEvent(event);
        return;
    }

    QSlider::paintEvent(event);
    const QImage sheet = skin.sheet(isVolume ? QStringLiteral("volume")
                                             : QStringLiteral("balance"));
    if (sheet.isNull() || sheet.width() < 68 || sheet.height() < 433)
        return;

    const int trackWidth = isVolume ? 68 : 38;
    const int position = isVolume
        ? (27 * (sliderPosition() - minimum())) / std::max(1, maximum() - minimum())
        : (27 * std::abs(sliderPosition()))
              / std::max(1, std::max(std::abs(minimum()), std::abs(maximum())));
    const QRect trackSource(isVolume ? 0 : 9,
                            std::clamp(position, 0, 27) * 15,
                            trackWidth, 13);
    const int trackX = (width() - trackWidth) / 2;
    const int trackY = (height() - 13) / 2;
    const int handleX = trackX + QStyle::sliderPositionFromValue(
        minimum(), maximum(), sliderPosition(), std::max(0, trackWidth - 14), false);
    const QRect handleSource(isSliderDown() ? 0 : 15, 422, 14, 11);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRect(trackX, trackY, trackWidth, 13), sheet, trackSource);
    painter.drawImage(QRect(handleX, (height() - 11) / 2, 14, 11), sheet, handleSource);
}

void LinkampSlider::setGradient(QColor from, QColor to, Qt::Orientation orientation)
{
    setOrientation(orientation);
    // create linear gradient
    QLinearGradient linearGrad(QPointF(0, 0), (orientation==Qt::Horizontal) ? QPointF(100, 0) : QPointF(0, 100));
    linearGrad.setColorAt(0, from);
    linearGrad.setColorAt(1, to);

    // paint gradient in a QImage:
    QPainter p(&gradient);
    p.fillRect(gradient.rect(), linearGrad);

    connect(this, SIGNAL(valueChanged(int)), this, SLOT(changeColor(int)));

    // initialize
    changeColor(value());
}

void LinkampSlider::setGradient(QList<QColor> steps, Qt::Orientation orientation)
{
    setOrientation(orientation);
    // create linear gradient
    QLinearGradient linearGrad(QPointF(0, 0), (orientation==Qt::Horizontal) ? QPointF(100, 0) : QPointF(0, 100));
    for(int i = 0; i < steps.count(); i++) {
        linearGrad.setColorAt(float(i)/float(steps.count() - 1), steps[i]);
    }

    // paint gradient in a QImage:
    QPainter p(&gradient);
    p.fillRect(gradient.rect(), linearGrad);

    connect(this, SIGNAL(valueChanged(int)), this, SLOT(changeColor(int)));

    // initialize
    changeColor(value());
}

void LinkampSlider::changeColor(int pos)
{
    QColor color;

    if (orientation() == Qt::Horizontal)
    {
        // retrieve color index based on cursor position
        int posIndex = gradient.size().width() * (pos - minimum()) / (maximum() - minimum());
        posIndex = std::min(posIndex, gradient.width() - 1);

        // pickup appropriate color
        color = gradient.pixel(posIndex, gradient.size().height()/2);
    }
    else
    {
        // retrieve color index based on cursor position
        int posIndex = gradient.size().height() * (pos - minimum()) / (maximum() - minimum());
        posIndex = std::min(posIndex, gradient.height() - 1);

        // pickup appropriate color
        color = gradient.pixel(gradient.size().width()/2, posIndex);
    }

    // create and apply stylesheet!
    // can be customized to change background and handle border!

    if(baseStylesheet.length() == 0) {
        // Capture original unmodified stylesheet the first time
        baseStylesheet = styleSheet();
    }

    // Append background color
    QString stylesheet = baseStylesheet +
        "QSlider::sub-page:" + ((orientation() == Qt::Horizontal) ? QString("horizontal"):QString("vertical")) + "{ \
            background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop: 0 " + color.darker(130).name() + ", stop: 1 " + color.name() + "); \
        }" +
        "QSlider::add-page:" + ((orientation() == Qt::Horizontal) ? QString("horizontal"):QString("vertical")) + "{ \
            background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop: 0 " + color.darker(130).name() + ", stop: 1 " + color.name() + "); \
        }";

    WinampSkin &skin = WinampSkin::instance();
    const bool isVolume = objectName() == QLatin1String("volumeSlider");
    const bool isBalance = objectName() == QLatin1String("balanceSlider");
    // In the built-in Classic skin, both controls use Winamp's fixed sprite
    // tracks. Repainting them with the generic audio gradient on each volume
    // update makes the track change or disappear as playback starts.
    if ((isVolume || isBalance) && !skin.isLoaded()) {
        return;
    }
    if (skin.isLoaded() && (isVolume || isBalance)) {
        const int position = isVolume
            ? (maximum() > minimum()
                   ? (27 * (pos - minimum())) / (maximum() - minimum()) : 0)
            : (27 * std::abs(pos))
                  / std::max(1, std::max(std::abs(minimum()), std::abs(maximum())));
        const QString alias = QStringLiteral("%1-track-%2.png")
                                  .arg(isVolume ? QStringLiteral("volume") : QStringLiteral("balance"))
                                  .arg(std::clamp(position, 0, 27));
        const QString trackPath = skin.assetPath(alias);
        if (!trackPath.isEmpty()) {
            stylesheet += QStringLiteral(
                "QSlider::groove:horizontal { height: 13px; border: 0; border-image: url(%1); }"
                "QSlider::sub-page:horizontal, QSlider::add-page:horizontal { background: transparent; border: 0; }")
                .arg(trackPath);
        }
    }
    setStyleSheet(skin.applyToStylesheet(stylesheet));
}
