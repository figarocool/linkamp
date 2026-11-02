#include "titlebar.h"
#include "ui_titlebar.h"
#include "scale.h"
#include <QMouseEvent>
#include <QPainter>

#include <utility>

TitleBar::TitleBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TitleBar)
{
    ui->setupUi(this);
    scale();
    setStyleSheet("TitleBar { background: transparent; }");
    ui->lineLT->hide();
    ui->lineLB->hide();
    ui->lineRT->hide();
    ui->lineRB->hide();
    ui->decorationL->hide();
    ui->decorationR->hide();
    ui->windowTitle->hide();
    connect(ui->menuButton, &QPushButton::clicked, this, &TitleBar::menuClicked);
    connect(ui->shadeButton, &QPushButton::clicked, this, &TitleBar::shadeClicked);
    connect(ui->closeButton, &QPushButton::clicked, window(), &QWidget::close);
    connect(ui->minimizeButton, &QPushButton::clicked, window(), &QWidget::showMinimized);
}

TitleBar::~TitleBar()
{
    delete ui;
}

void TitleBar::setTitle(const QString &title)
{
    ui->windowTitle->setText(title);
}

void TitleBar::setClassicEqualizerStyle()
{
    m_classicEqualizerStyle = true;
    ui->menuButton->hide();
    ui->minimizeButton->hide();
    ui->shadeButton->hide();
    ui->closeButton->hide();
    ui->windowTitle->hide();
    ui->decorationL->hide();
    ui->decorationR->hide();
    setStyleSheet(QStringLiteral("TitleBar { background: transparent; }") );
    update();
}


void TitleBar::scale()
{
    QRect geo = this->geometry();
    geo.setHeight(geo.height() * UI_SCALE);
    this->setGeometry(geo);

    ui->closeButton->setMaximumSize(ui->closeButton->maximumSize() * UI_SCALE);
    ui->closeButton->setMinimumSize(ui->closeButton->minimumSize() * UI_SCALE);

    ui->menuButton->setMaximumSize(ui->menuButton->maximumSize() * UI_SCALE);
    ui->menuButton->setMinimumSize(ui->menuButton->minimumSize() * UI_SCALE);

    ui->shadeButton->setMaximumSize(ui->shadeButton->maximumSize() * UI_SCALE);
    ui->shadeButton->setMinimumSize(ui->shadeButton->minimumSize() * UI_SCALE);

    ui->minimizeButton->setMaximumSize(ui->minimizeButton->maximumSize() * UI_SCALE);
    ui->minimizeButton->setMinimumSize(ui->minimizeButton->minimumSize() * UI_SCALE);

    QFont titleFont = ui->windowTitle->font();
    titleFont.setPointSize(titleFont.pointSize() * UI_SCALE);
    ui->windowTitle->setFont(titleFont);

    ui->decorationL->layout()->setContentsMargins(ui->decorationL->layout()->contentsMargins() * UI_SCALE);
    ui->decorationL->layout()->setSpacing(ui->decorationL->layout()->spacing() * UI_SCALE);

    ui->decorationR->layout()->setContentsMargins(ui->decorationR->layout()->contentsMargins() * UI_SCALE);
    ui->decorationR->layout()->setSpacing(ui->decorationR->layout()->spacing() * UI_SCALE);

    ui->lineLT->setMaximumHeight(ui->lineLT->maximumHeight() * UI_SCALE);
    ui->lineLT->setMinimumHeight(ui->lineLT->minimumHeight() * UI_SCALE);

    ui->lineLB->setMaximumHeight(ui->lineLB->maximumHeight() * UI_SCALE);
    ui->lineLB->setMinimumHeight(ui->lineLB->minimumHeight() * UI_SCALE);

    ui->lineRT->setMaximumHeight(ui->lineRT->maximumHeight() * UI_SCALE);
    ui->lineRT->setMinimumHeight(ui->lineRT->minimumHeight() * UI_SCALE);

    ui->lineRB->setMaximumHeight(ui->lineRB->maximumHeight() * UI_SCALE);
    ui->lineRB->setMinimumHeight(ui->lineRB->minimumHeight() * UI_SCALE);

    QSize sh = ui->horizontalSpacer->sizeHint();
    QSizePolicy sp = ui->horizontalSpacer->sizePolicy();
    ui->horizontalSpacer->changeSize(sh.width()*UI_SCALE, sh.height(), sp.horizontalPolicy(), sp.verticalPolicy());

    QSize sh2 = ui->horizontalSpacer_2->sizeHint();
    QSizePolicy sp2 = ui->horizontalSpacer_2->sizePolicy();
    ui->horizontalSpacer_2->changeSize(sh2.width()*UI_SCALE, sh2.height(), sp2.horizontalPolicy(), sp2.verticalPolicy());

    QSize sh4 = ui->horizontalSpacer_4->sizeHint();
    QSizePolicy sp4 = ui->horizontalSpacer_4->sizePolicy();
    ui->horizontalSpacer_4->changeSize(sh4.width()*UI_SCALE, sh4.height(), sp4.horizontalPolicy(), sp4.verticalPolicy());
}

void TitleBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_classicEqualizerStyle) {
        ui->menuButton->setGeometry(6 * UI_SCALE, 3 * UI_SCALE, 9 * UI_SCALE, 9 * UI_SCALE);
        ui->minimizeButton->setGeometry(244 * UI_SCALE, 3 * UI_SCALE, 9 * UI_SCALE, 9 * UI_SCALE);
        ui->shadeButton->setGeometry(254 * UI_SCALE, 3 * UI_SCALE, 9 * UI_SCALE, 9 * UI_SCALE);
        ui->closeButton->setGeometry(264 * UI_SCALE, 3 * UI_SCALE, 9 * UI_SCALE, 9 * UI_SCALE);
    }
}

void TitleBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        update();
    }
    QWidget::changeEvent(event);
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    if (!m_classicEqualizerStyle) {
        const QPixmap sprite(QStringLiteral(":/assets/classic-titlebar.png"));
        if (!sprite.isNull()) {
            QPainter painter(this);
            const int titleY = (window() && window()->isActiveWindow()) ? 0 : 14;
            painter.drawPixmap(rect(), sprite, QRect(0, titleY, 275, 14));
            return;
        }
        QWidget::paintEvent(event);
        return;
    }

    Q_UNUSED(event);
    const QPixmap sprite(QStringLiteral(":/assets/classic-eqmain.png"));
    if (sprite.isNull())
        return;

    QPainter painter(this);
    const int titleY = window()->isActiveWindow() ? 134 : 149;
    painter.drawPixmap(rect(), sprite, QRect(0, titleY, 275, 14));
    painter.drawPixmap(254, 3, 9, 9, sprite, 254, 137, 9, 9);
    painter.drawPixmap(264, 3, 9, 9, sprite, 0, 116, 9, 9);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (m_classicEqualizerStyle && event->button() == Qt::LeftButton) {
        const QPoint position = event->position().toPoint();
        m_pressedEqualizerClose = position.x() >= 264 && position.y() <= 12;
        m_pressedEqualizerShade = position.x() >= 254 && position.x() < 263
                                  && position.y() <= 12;
        if (m_pressedEqualizerClose || m_pressedEqualizerShade) {
            event->accept();
            return;
        }
    }
    if(event->button() == Qt::LeftButton)
    {
        m_dragging = true;
        emit moveStarted(event->modifiers() & Qt::ShiftModifier);
        m_pCursor = event->globalPosition() - window()->geometry().topLeft();
        event->accept();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if(event->buttons() & Qt::LeftButton)
    {
        window()->move((event->globalPosition() - m_pCursor).toPoint());
        event->accept();
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_classicEqualizerStyle && event->button() == Qt::LeftButton) {
        const QPoint position = event->position().toPoint();
        const bool closePressed = std::exchange(m_pressedEqualizerClose, false);
        const bool shadePressed = std::exchange(m_pressedEqualizerShade, false);
        if (closePressed && position.x() >= 264 && position.y() <= 12)
            window()->close();
        else if (shadePressed && position.x() >= 254 && position.x() < 263
                 && position.y() <= 12)
            emit shadeClicked();
        if (closePressed || shadePressed) {
            event->accept();
            return;
        }
    }
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        emit moveFinished();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
