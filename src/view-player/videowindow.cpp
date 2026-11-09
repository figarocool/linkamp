#include "videowindow.h"

#include "winampskin.h"

#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QVector>

#include <algorithm>

namespace {
constexpr int kMinWindowWidth = 275;
constexpr int kMinWindowHeight = 116;
constexpr int kDefaultWindowHeight = 203;
constexpr int kTitleBarHeight = 20;
constexpr int kBottomBarHeight = 14;
constexpr int kLeftBorderWidth = 11;
constexpr int kRightBorderWidth = 8;

struct GlyphSpan {
    int x = 0;
    int width = 0;
};

QVector<GlyphSpan> parseGenFontRow(const QImage &genSheet, int rowY)
{
    QVector<GlyphSpan> spans;
    if (genSheet.isNull() || rowY < 0 || rowY + 7 > genSheet.height())
        return spans;

    const QRgb sepColor = genSheet.pixel(0, rowY) & 0x00ffffff;
    bool inGlyph = false;
    int startX = 0;
    for (int x = 0; x < genSheet.width(); ++x) {
        const bool isSep = ((genSheet.pixel(x, rowY) & 0x00ffffff) == sepColor);
        if (!isSep && !inGlyph) {
            inGlyph = true;
            startX = x;
        } else if (isSep && inGlyph) {
            inGlyph = false;
            spans.append({startX, x - startX});
            if (spans.size() >= 26)
                break;
        }
    }
    return spans;
}

void drawGenTitleText(QPainter &painter, const QPixmap &genPixmap,
                      const QImage &genImage, const QString &text,
                      int capsuleX, int capsuleWidth, bool active)
{
    const int fontY = active ? 88 : 96;
    const QVector<GlyphSpan> glyphs = parseGenFontRow(genImage, fontY);
    if (glyphs.size() < 26)
        return;

    int totalWidth = 0;
    for (QChar ch : text) {
        const QChar upper = ch.toUpper();
        if (upper >= QLatin1Char('A') && upper <= QLatin1Char('Z')) {
            const int idx = upper.unicode() - QLatin1Char('A').unicode();
            totalWidth += glyphs[idx].width;
        } else if (upper == QLatin1Char(' ')) {
            totalWidth += 4;
        }
    }

    int curX = capsuleX + (capsuleWidth - totalWidth) / 2;
    for (QChar ch : text) {
        const QChar upper = ch.toUpper();
        if (upper >= QLatin1Char('A') && upper <= QLatin1Char('Z')) {
            const int idx = upper.unicode() - QLatin1Char('A').unicode();
            const GlyphSpan span = glyphs[idx];
            painter.drawPixmap(QRect(curX, 4, span.width, 7), genPixmap,
                               QRect(span.x, fontY, span.width, 7));
            curX += span.width;
        } else if (upper == QLatin1Char(' ')) {
            curX += 4;
        }
    }
}
} // namespace

VideoWindow::VideoWindow(QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_videoSink(new QVideoSink(this))
{
    setWindowTitle(QStringLiteral("Linkamp Video"));
    setMinimumSize(kMinWindowWidth, kMinWindowHeight);

    const QSettings settings;
    const QSize savedSize(
        settings.value(QStringLiteral("video/width"), kMinWindowWidth).toInt(),
        settings.value(QStringLiteral("video/height"), kDefaultWindowHeight).toInt());
    resize(qMax(kMinWindowWidth, savedSize.width()),
           qMax(kMinWindowHeight, savedSize.height()));

    const QString savedPositionKey = QStringLiteral("windows/video/position");
    if (settings.contains(savedPositionKey)) {
        move(settings.value(savedPositionKey).toPoint());
        setProperty("savedWindowPosition", true);
    }

    setMouseTracking(true);
    m_clock.start();

    connect(m_videoSink, &QVideoSink::videoFrameChanged,
            this, &VideoWindow::handleVideoFrame);
}

void VideoWindow::refreshSkin()
{
    update();
}

void VideoWindow::setPlaybackState(MediaPlayer::PlaybackState state)
{
    m_playbackState = state;
    if (m_playbackState == MediaPlayer::StoppedState) {
        m_currentFrame = QImage();
    }
    if (isVisible())
        update();
}

void VideoWindow::setCurrentTrackTitle(const QString &title)
{
    const QString trimmed = title.trimmed();
    if (m_trackTitle == trimmed)
        return;
    m_trackTitle = trimmed;
    if (isVisible() && m_currentFrame.isNull())
        update();
}

void VideoWindow::handleVideoFrame(const QVideoFrame &frame)
{
    if (!frame.isValid()) {
        return;
    }
    QImage img = frame.toImage();
    if (!img.isNull()) {
        m_currentFrame = std::move(img);
        if (isVisible())
            update();
    }
}

QRect VideoWindow::canvasRect() const
{
    if (m_fullScreen)
        return rect();
    return QRect(kLeftBorderWidth,
                 kTitleBarHeight,
                 std::max(1, width() - kLeftBorderWidth - kRightBorderWidth),
                 std::max(1, height() - kTitleBarHeight - kBottomBarHeight));
}

QRect VideoWindow::closeButtonRect() const
{
    if (m_fullScreen)
        return QRect();
    return QRect(width() - 11, 3, 9, 9);
}

void VideoWindow::toggleFullScreenMode()
{
    m_lastFullScreenToggleMs = m_clock.elapsed();
    m_dragging = false;
    m_resizing = false;
    m_closePressed = false;

    if (!m_fullScreen) {
        m_windowedGeometry = geometry();
        m_fullScreen = true;
        QScreen *targetScreen = screen();
        if (!targetScreen)
            targetScreen = QGuiApplication::primaryScreen();
        const QRect screenGeo = targetScreen ? targetScreen->geometry() : QRect(0, 0, 1920, 1080);
        setMinimumSize(1, 1);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setGeometry(screenGeo);
        show();
        raise();
        activateWindow();
        setFocus();
    } else {
        m_fullScreen = false;
        setMinimumSize(kMinWindowWidth, kMinWindowHeight);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        showNormal();
        if (m_windowedGeometry.isValid()
            && m_windowedGeometry.width() >= kMinWindowWidth
            && m_windowedGeometry.height() >= kMinWindowHeight) {
            setGeometry(m_windowedGeometry);
        } else {
            resize(kMinWindowWidth, kDefaultWindowHeight);
        }
        show();
        raise();
        activateWindow();
        setFocus();
    }
    update();
}

void VideoWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRect canvas = canvasRect();
    painter.fillRect(canvas, Qt::black);

    if (!m_currentFrame.isNull()) {
        const QSize scaledSize = m_currentFrame.size().scaled(canvas.size(), Qt::KeepAspectRatio);
        const QRect targetRect(
            canvas.x() + (canvas.width() - scaledSize.width()) / 2,
            canvas.y() + (canvas.height() - scaledSize.height()) / 2,
            scaledSize.width(),
            scaledSize.height());
        painter.save();
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(targetRect, m_currentFrame);
        painter.restore();
    } else {
        painter.save();
        painter.setClipRect(canvas);
        QFont titleFont(QStringLiteral("DejaVu Sans"), m_fullScreen ? 16 : 9, QFont::Bold);
        titleFont.setStyleStrategy(QFont::NoAntialias);
        painter.setFont(titleFont);
        painter.setPen(QColor(0, 210, 80));
        const QRect headerRect(canvas.x() + 8, canvas.center().y() - (m_fullScreen ? 28 : 18),
                               canvas.width() - 16, m_fullScreen ? 26 : 16);
        painter.drawText(headerRect, Qt::AlignCenter, QStringLiteral("LINKAMP VIDEO"));

        QFont subFont(QStringLiteral("DejaVu Sans"), m_fullScreen ? 12 : 8, QFont::Normal);
        subFont.setStyleStrategy(QFont::NoAntialias);
        painter.setFont(subFont);
        painter.setPen(QColor(180, 195, 210));
        const QString statusText = !m_trackTitle.isEmpty()
            ? m_trackTitle
            : tr("Nessun flusso video attivo");
        const QRect subRect(canvas.x() + 10, canvas.center().y() + 2,
                            canvas.width() - 20, m_fullScreen ? 24 : 16);
        const QFontMetrics fm(subFont);
        painter.drawText(subRect, Qt::AlignCenter,
                         fm.elidedText(statusText, Qt::ElideRight, subRect.width()));
        painter.restore();
    }

    if (m_fullScreen)
        return;

    const QImage genImage = WinampSkin::instance().sheet(QStringLiteral("gen"));
    const QPixmap genPixmap = QPixmap::fromImage(genImage);
    if (genPixmap.isNull())
        return;

    auto blit = [&painter, &genPixmap](int x, int y, int w, int h,
                                       int sx, int sy, int sw, int sh) {
        painter.drawPixmap(QRect(x, y, w, h), genPixmap, QRect(sx, sy, sw, sh));
    };

    const bool active = window() && window()->isActiveWindow();
    const int titleY = active ? 0 : 21;

    constexpr int capsuleInteriorWidth = 75;
    const int goldTotalWidth = std::max(0, width() - 50 - 50 - capsuleInteriorWidth);
    const int leftGoldWidth = (goldTotalWidth / 50) * 25;
    const int rightGoldWidth = goldTotalWidth - leftGoldWidth;

    int x = 0;
    blit(x, 0, 25, kTitleBarHeight, 0, titleY, 25, kTitleBarHeight);
    x += 25;

    for (int gx = 0; gx < leftGoldWidth; gx += 25) {
        const int sliceW = std::min(25, leftGoldWidth - gx);
        blit(x, 0, sliceW, kTitleBarHeight, 104, titleY, sliceW, kTitleBarHeight);
        x += sliceW;
    }

    blit(x, 0, 25, kTitleBarHeight, 26, titleY, 25, kTitleBarHeight);
    x += 25;

    const int capsuleStartX = x;
    for (int cx = 0; cx < capsuleInteriorWidth; cx += 25) {
        blit(x, 0, 25, kTitleBarHeight, 52, titleY, 25, kTitleBarHeight);
        x += 25;
    }

    blit(x, 0, 25, kTitleBarHeight, 78, titleY, 25, kTitleBarHeight);
    x += 25;

    for (int gx = 0; gx < rightGoldWidth; gx += 25) {
        const int sliceW = std::min(25, rightGoldWidth - gx);
        blit(x, 0, sliceW, kTitleBarHeight, 104, titleY, sliceW, kTitleBarHeight);
        x += sliceW;
    }

    blit(width() - 25, 0, 25, kTitleBarHeight, 130, titleY, 25, kTitleBarHeight);
    if (m_closePressed) {
        blit(width() - 11, 3, 9, 9, 148, 42, 9, 9);
    }

    drawGenTitleText(painter, genPixmap, genImage, QStringLiteral("VIDEO"),
                     capsuleStartX, capsuleInteriorWidth, active);

    // Side borders
    const int sideHeight = height() - kTitleBarHeight - kBottomBarHeight;
    for (int by = 0; by < sideHeight; by += 29) {
        const int sliceH = std::min(29, sideHeight - by);
        blit(0, kTitleBarHeight + by, kLeftBorderWidth, sliceH, 127, 42, kLeftBorderWidth, sliceH);
        blit(width() - kRightBorderWidth, kTitleBarHeight + by, kRightBorderWidth, sliceH,
             139, 42, kRightBorderWidth, sliceH);
    }

    // Bottom border
    const int bottomY = height() - kBottomBarHeight;
    blit(0, bottomY, 125, kBottomBarHeight, 0, 42, 125, kBottomBarHeight);
    const int midBottomWidth = std::max(0, width() - 250);
    for (int bx = 0; bx < midBottomWidth; bx += 25) {
        const int sliceW = std::min(25, midBottomWidth - bx);
        blit(125 + bx, bottomY, sliceW, kBottomBarHeight, 127, 72, sliceW, kBottomBarHeight);
    }
    blit(width() - 125, bottomY, 125, kBottomBarHeight, 0, 57, 125, kBottomBarHeight);
}

void VideoWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    update();
}

void VideoWindow::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
}

void VideoWindow::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (!m_fullScreen) {
        QSettings settings;
        settings.setValue(QStringLiteral("video/width"), width());
        settings.setValue(QStringLiteral("video/height"), height());
    }
}

void VideoWindow::moveEvent(QMoveEvent *event)
{
    QDialog::moveEvent(event);
    if (isVisible() && !m_fullScreen) {
        QSettings().setValue(QStringLiteral("windows/video/position"),
                             frameGeometry().topLeft());
    }
}

void VideoWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange)
        update();
    QDialog::changeEvent(event);
}

void VideoWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        showContextMenu(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QDialog::mousePressEvent(event);
        return;
    }

    const qint64 nowMs = m_clock.elapsed();
    if (m_fullScreen) {
        if (nowMs - m_lastFullScreenToggleMs >= 300) {
            toggleFullScreenMode();
        }
        event->accept();
        return;
    }

    const QPoint pos = event->position().toPoint();
    if (closeButtonRect().contains(pos)) {
        m_closePressed = true;
        update();
        event->accept();
        return;
    }

    if (QRect(width() - 20, height() - 20, 20, 20).contains(pos)) {
        m_resizing = true;
        m_resizeStartGlobal = event->globalPosition().toPoint();
        m_resizeStartSize = size();
        event->accept();
        return;
    }

    if (pos.y() < kTitleBarHeight) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        emit moveStarted(event->modifiers().testFlag(Qt::ShiftModifier));
        event->accept();
        return;
    }

    if (canvasRect().contains(pos)) {
        if (nowMs - m_lastFullScreenToggleMs >= 300 && (nowMs - m_lastCanvasClickMs) <= 450) {
            m_lastCanvasClickMs = -1000;
            toggleFullScreenMode();
            event->accept();
            return;
        }
        m_lastCanvasClickMs = nowMs;
        event->accept();
        return;
    }

    QDialog::mousePressEvent(event);
}

void VideoWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    if (m_resizing) {
        const QPoint delta = event->globalPosition().toPoint() - m_resizeStartGlobal;
        const int rawW = std::max(kMinWindowWidth, m_resizeStartSize.width() + delta.x());
        const int rawH = std::max(kMinWindowHeight, m_resizeStartSize.height() + delta.y());
        const int steppedW = kMinWindowWidth + ((rawW - kMinWindowWidth + 12) / 25) * 25;
        const int steppedH = kMinWindowHeight + ((rawH - kMinWindowHeight + 14) / 29) * 29;
        resize(steppedW, steppedH);
        event->accept();
        return;
    }

    if (!m_fullScreen && QRect(width() - 20, height() - 20, 20, 20).contains(pos))
        setCursor(Qt::SizeFDiagCursor);
    else
        unsetCursor();

    QDialog::mouseMoveEvent(event);
}

void VideoWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_closePressed) {
        const bool shouldClose = closeButtonRect().contains(event->position().toPoint());
        m_closePressed = false;
        update();
        if (shouldClose) {
            QSettings().setValue(QStringLiteral("windows/video/visible"), false);
            hide();
        }
        event->accept();
        return;
    }
    if (m_dragging) {
        m_dragging = false;
        emit moveFinished();
        event->accept();
        return;
    }
    if (m_resizing) {
        m_resizing = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

void VideoWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !closeButtonRect().contains(event->position().toPoint())) {
        if (m_clock.elapsed() - m_lastFullScreenToggleMs >= 250) {
            m_lastCanvasClickMs = -1000;
            toggleFullScreenMode();
        }
        event->accept();
        return;
    }
    QDialog::mouseDoubleClickEvent(event);
}

void VideoWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_F:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        toggleFullScreenMode();
        event->accept();
        return;
    case Qt::Key_Escape:
        if (m_fullScreen) {
            toggleFullScreenMode();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }
    QDialog::keyPressEvent(event);
}

void VideoWindow::showContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *fullAct = menu.addAction(tr("Schermo intero (Doppio clic / F)"));
    fullAct->setCheckable(true);
    fullAct->setChecked(m_fullScreen);
    menu.addSeparator();
    QAction *size1xAct = menu.addAction(tr("Dimensione 1x (275x203)"));
    QAction *size2xAct = menu.addAction(tr("Dimensione 2x (550x406)"));
    menu.addSeparator();
    QAction *closeAct = menu.addAction(tr("Chiudi Video"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == fullAct) {
        toggleFullScreenMode();
    } else if (chosen == size1xAct) {
        if (m_fullScreen)
            toggleFullScreenMode();
        resize(275, 203);
    } else if (chosen == size2xAct) {
        if (m_fullScreen)
            toggleFullScreenMode();
        resize(550, 406);
    } else if (chosen == closeAct) {
        QSettings().setValue(QStringLiteral("windows/video/visible"), false);
        hide();
    }
}
