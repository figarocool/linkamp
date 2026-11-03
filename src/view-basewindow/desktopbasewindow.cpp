#include "desktopbasewindow.h"
#include "ui_desktopbasewindow.h"
#include "scale.h"
#include "winampskin.h"
#include <QPainter>
#include <QPixmap>
#include <QSettings>

DesktopBaseWindow::DesktopBaseWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DesktopBaseWindow)
{
    ui->setupUi(this);
    scale();
    const QSettings settings;
    const QString positionKey = QStringLiteral("windows/main/position");
    if (settings.contains(positionKey))
        move(settings.value(positionKey).toPoint());
}

DesktopBaseWindow::~DesktopBaseWindow()
{
    delete ui;
}

void DesktopBaseWindow::scale()
{
    this->setBaseSize(this->baseSize() * UI_SCALE);
    this->layout()->setContentsMargins(this->layout()->contentsMargins() * UI_SCALE);

    ui->bodyContainer->layout()->setContentsMargins(ui->bodyContainer->layout()->contentsMargins() * UI_SCALE);
    ui->bodyOuterFrame->layout()->setContentsMargins(ui->bodyOuterFrame->layout()->contentsMargins() * UI_SCALE);

    ui->titlebarContainer->setMaximumHeight(ui->titlebarContainer->maximumHeight() * UI_SCALE);
    ui->titlebarContainer->setMinimumHeight(ui->titlebarContainer->minimumHeight() * UI_SCALE);

    this->setStyleSheet(getStylesheet("desktopbasewindow"));
}

void DesktopBaseWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    if (isVisible())
        QSettings().setValue(QStringLiteral("windows/main/position"), pos());
}

void DesktopBaseWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (WinampSkin::instance().isLoaded())
        return;

    const QPixmap mainSheet = QPixmap::fromImage(
        WinampSkin::instance().sheet(QStringLiteral("main")));
    const QPixmap linkampMark(QStringLiteral(":/assets/linkamp-mark.png"));
    if (mainSheet.isNull() || linkampMark.isNull())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRect boltArea(252 * UI_SCALE, 92 * UI_SCALE, 13 * UI_SCALE, 14 * UI_SCALE);
    // Copy the nearby empty panel texture over the bundled lightning, then
    // place Linkamp's transparent mark in the same lower-right slot.
    painter.drawPixmap(boltArea, mainSheet, QRect(239, 92, 13, 14));
    painter.drawPixmap(boltArea, linkampMark);
}
