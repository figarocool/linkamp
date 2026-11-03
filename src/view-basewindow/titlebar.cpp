#include "titlebar.h"
#include "ui_titlebar.h"
#include "scale.h"
#include "winampskin.h"
#include <QMouseEvent>
#include <QPainter>
#include <QFont>

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
    m_title = title.toUpper();
    ui->windowTitle->setText(title);
    update();
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

void TitleBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (window()) {
        window()->installEventFilter(this);
    }
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == window() && (event->type() == QEvent::ActivationChange || event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)) {
        update();
    }
    return QWidget::eventFilter(watched, event);
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    if (!m_classicEqualizerStyle) {
        const QPixmap sprite = QPixmap::fromImage(
            WinampSkin::instance().sheet(QStringLiteral("classic-titlebar")));
        if (!sprite.isNull()) {
            QPainter painter(this);
            const int titleY = (window() && window()->isActiveWindow()) ? 0 : 14;
            painter.drawPixmap(rect(), sprite, QRect(0, titleY, 275, 14));
            if (!WinampSkin::instance().isLoaded() && titleY != 0) {
                // The bundled inactive titlebar atlas has a stray square in
                // this otherwise plain rail. Reuse the adjacent rail pixels.
                painter.drawPixmap(QRect(59, 0, 22, 14), sprite,
                                   QRect(80, titleY, 22, 14));
            }
            if (!WinampSkin::instance().isLoaded()) {
                const QPixmap linkampMark(QStringLiteral(":/assets/linkamp-mark.png"));
                if (!linkampMark.isNull()) {
                    painter.save();
                    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    painter.drawPixmap(QRect(14, 2, 8, 8), linkampMark);
                    painter.restore();
                }
            }

            // Keep the original Winamp titlebar artwork while using Linkamp's
            // name in the logo slot baked into the classic sprite.
            if (!m_title.isEmpty()) {
                painter.save();
                painter.scale(width() / 275.0, height() / 14.0);
                painter.fillRect(QRect(111, 4, 53, 7), QColor(24, 24, 41));
                painter.setRenderHint(QPainter::TextAntialiasing, false);
                QFont titleFont(QStringLiteral("DejaVu Sans Mono"));
                titleFont.setPixelSize(8);
                titleFont.setBold(true);
                titleFont.setStyleStrategy(QFont::NoAntialias);
                painter.setFont(titleFont);
                painter.setPen(QColor(192, 192, 192));
                painter.drawText(QRect(111, 1, 53, 12), Qt::AlignCenter, m_title);
                painter.restore();
            }
            return;
        }
        QWidget::paintEvent(event);
        return;
    }

    Q_UNUSED(event);
    const QPixmap sprite = QPixmap::fromImage(
        WinampSkin::instance().sheet(QStringLiteral("eqmain")));
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
