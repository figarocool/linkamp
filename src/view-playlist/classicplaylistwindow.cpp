#include "classicplaylistwindow.h"

#include "util.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QUrl>
#include <QWheelEvent>
#include <QKeyEvent>

#include <algorithm>
#include <functional>

namespace {
constexpr int kWindowWidth = 275;
constexpr int kWindowHeight = 116;
constexpr int kListTop = 21;
constexpr int kListBottomInset = 38;
constexpr int kRowHeight = 12;
}

ClassicPlaylistWindow::ClassicPlaylistWindow(QMediaPlaylist *playlist, QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_playlist(playlist)
{
    setWindowTitle(QStringLiteral("Linkamp Playlist"));
    setMinimumSize(kWindowWidth, kWindowHeight);
    resize(kWindowWidth, kWindowHeight);
    setMouseTracking(true);

    connect(m_playlist, &QMediaPlaylist::mediaInserted, this, &ClassicPlaylistWindow::updatePlaylist);
    connect(m_playlist, &QMediaPlaylist::mediaRemoved, this, &ClassicPlaylistWindow::updatePlaylist);
    connect(m_playlist, &QMediaPlaylist::mediaChanged, this, &ClassicPlaylistWindow::updatePlaylist);
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, &ClassicPlaylistWindow::updatePlaylist);
}

int ClassicPlaylistWindow::rowAt(const QPoint &position) const
{
    if (position.y() < kListTop || position.y() >= height() - kListBottomInset)
        return -1;
    const int row = m_firstVisibleRow + (position.y() - kListTop) / kRowHeight;
    return row >= 0 && row < m_playlist->mediaCount() ? row : -1;
}

void ClassicPlaylistWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    const QPixmap sprite(QStringLiteral(":/assets/classic-pledit.png"));
    if (sprite.isNull()) {
        painter.fillRect(rect(), QColor(12, 14, 24));
        return;
    }

    auto blit = [&painter, &sprite](int x, int y, int width, int height,
                                  int sourceX, int sourceY, int sourceWidth, int sourceHeight) {
        painter.drawPixmap(QRect(x, y, width, height), sprite,
                           QRect(sourceX, sourceY, sourceWidth, sourceHeight));
    };
    auto drawTitle = [&painter](const QRect &titleRect, int fontPixels) {
        QLinearGradient gradient(titleRect.topLeft(), titleRect.bottomLeft());
        gradient.setColorAt(0.0, QColor(55, 53, 65));
        gradient.setColorAt(1.0, QColor(28, 27, 37));
        painter.fillRect(titleRect, gradient);
        QFont titleFont(QStringLiteral("DejaVu Sans Mono"));
        titleFont.setPixelSize(fontPixels);
        titleFont.setBold(true);
        titleFont.setStyleStrategy(QFont::NoAntialias);
        painter.setFont(titleFont);
        painter.setPen(Qt::white);
        painter.drawText(titleRect, Qt::AlignCenter, QStringLiteral("LINKAMP PLAYLIST"));
    };

    // Pledit.bmp is a sprite sheet; assemble its title bar and tiled frame
    // using the original sprite rectangles.
    if (m_windowShaded) {
        int x = 0;
        int tiles = (width() - 50) / 25;
        blit(x, 0, 25, 14, 72, 42, 25, 14);
        x += 25;
        --tiles;
        while (tiles-- > 0) {
            blit(x, 0, 25, 14, 72, 57, 25, 14);
            x += 25;
        }
        const int remainder = (width() - 50) % 25;
        if (remainder > 0) {
            blit(x, 0, remainder, 14, 72, 57, 25, 14);
            x += remainder;
        }
        blit(x, 0, 50, 14, 99, 42, 50, 14);
        blit(width() - 11, 3, 9, 9, 167, 3, 9, 9);
        blit(width() - 20, 3, 9, 9, 128, 45, 9, 9);
        drawTitle(QRect(25, 0, width() - 50, 14), 7);
        return;
    }

    int x = 0;
    int tiles = (width() - 25 - 25 - 100) / 25;
    blit(x, 0, 25, 20, 0, 0, 25, 20);
    x += 25;
    if (tiles & 1) {
        blit(x, 0, 12, 20, 127, 0, 25, 20);
        x += 12;
    }
    tiles /= 2;
    while (tiles-- > 0) {
        blit(x, 0, 25, 20, 127, 0, 25, 20);
        x += 25;
    }
    blit(x, 0, 100, 20, 26, 0, 100, 20);
    x += 100;
    tiles = (width() - 25 - 25 - 100) / 25;
    if (tiles & 1) {
        blit(x, 0, 13, 20, 127, 0, 25, 20);
        x += 13;
    }
    tiles /= 2;
    while (tiles-- > 0) {
        blit(x, 0, 25, 20, 127, 0, 25, 20);
        x += 25;
    }
    const int remainder = (width() - 25 - 25 - 100) % 25;
    if (remainder > 0) {
        blit(x, 0, remainder, 20, 127, 0, 25, 20);
        x += remainder;
    }
    blit(x, 0, 25, 20, 153, 0, 25, 20);
    blit(width() - 11, 3, 9, 9, 167, 3, 9, 9);
    blit(width() - 20, 3, 9, 9, 158, 3, 9, 9);
    drawTitle(QRect(25, 0, 100, 20), 8);

    const int trackHeight = height() - 20 - 38;
    int borderY = 20;
    while (borderY + 29 <= 20 + trackHeight) {
        blit(0, borderY, 12, 29, 0, 42, 12, 29);
        blit(width() - 20, borderY, 5, 29, 31, 42, 5, 29);
        blit(width() - 7, borderY, 7, 29, 44, 42, 7, 29);
        borderY += 29;
    }
    const int borderRemainder = 20 + trackHeight - borderY;
    if (borderRemainder > 0) {
        blit(0, borderY, 12, borderRemainder, 0, 42, 12, 29);
        blit(width() - 20, borderY, 5, borderRemainder, 31, 42, 5, 29);
        blit(width() - 7, borderY, 7, borderRemainder, 44, 42, 7, 29);
    }

    const int bottomY = height() - 38;
    blit(0, bottomY, 125, 38, 0, 72, 125, 38);
    int bottomX = 125;
    int middleWidth = width() - 125 - 150;
    int middleTiles = middleWidth / 25;
    const int extraPanel = middleTiles >= 3;
    if (extraPanel)
        middleTiles -= 3;
    for (int i = 0; i < middleTiles; ++i) {
        blit(bottomX, bottomY, 25, 38, 179, 0, 25, 38);
        bottomX += 25;
    }
    const int middleRemainder = middleWidth % 25;
    if (middleRemainder > 0) {
        blit(bottomX, bottomY, middleRemainder, 38, 179, 0, 25, 38);
        bottomX += middleRemainder;
    }
    if (extraPanel) {
        blit(bottomX, bottomY, 75, 38, 205, 0, 75, 38);
        bottomX += 75;
    }
    blit(bottomX, bottomY, width() - bottomX, 38, 126, 72, 150, 38);

    const int buttonY = height() - 30;
    // Draw the five idle buttons from the main Pledit sheet. Popup rows and
    // their vertical separators are drawn only while their menus are open.
    blit(11, buttonY, 25, 18, 11, 80, 25, 18);
    blit(40, buttonY, 25, 18, 40, 80, 25, 18);
    blit(69, buttonY, 25, 18, 69, 80, 25, 18);
    blit(98, buttonY, 25, 18, 98, 80, 25, 18);
    blit(width() - 46, buttonY, 25, 18, 229, 80, 25, 18);

    const int thumbHeight = 18;
    const int visibleRows = std::max(1, (height() - 38 - 20 - 2) / kRowHeight);
    const int maxFirstRow = std::max(0, m_playlist->mediaCount() - visibleRows);
    const int thumbY = maxFirstRow == 0
        ? 0 : ((trackHeight - thumbHeight) * m_firstVisibleRow) / maxFirstRow;
    for (int y = 0; y < trackHeight; y += 29) {
        const int height = std::min(29, trackHeight - y);
        blit(width() - 15, 20 + y, 8, height, 36, 42, 8, 29);
    }
    blit(width() - 15, 20 + thumbY, 8, thumbHeight, 52, 53, 9, 18);

    painter.setRenderHint(QPainter::TextAntialiasing, false);
    QFont font(QStringLiteral("DejaVu Sans Mono"));
    font.setPixelSize(8);
    font.setStyleStrategy(QFont::NoAntialias);
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const int end = std::min(m_playlist->mediaCount(), m_firstVisibleRow + visibleRows);

    for (int row = m_firstVisibleRow; row < end; ++row) {
        const int y = kListTop + (row - m_firstVisibleRow) * kRowHeight;
        const QRect rowRect(12, y, width() - 32, kRowHeight);
        const bool selected = m_selectedRows.contains(row);
        if (selected)
            painter.fillRect(rowRect, QColor(0, 190, 0));

        const QMediaMetaData metadata = m_playlist->mediaMetadata(row);
        QString title = metadata.value(QMediaMetaData::Title).toString();
        if (title.isEmpty())
            title = m_playlist->media(row).fileName();
        const QString artist = metadata.value(QMediaMetaData::AlbumArtist).toString();
        if (!artist.isEmpty())
            title = artist + QStringLiteral(" - ") + title;

        const QString track = QStringLiteral("%1.").arg(row + 1, 2, 10, QLatin1Char('0'));
        QString duration;
        const qint64 durationMs = metadata.value(QMediaMetaData::Duration).toLongLong();
        if (durationMs > 0)
            duration = formatDuration(durationMs);

        const bool current = row == m_playlist->currentIndex();
        painter.setPen(selected ? Qt::black : (current ? QColor(255, 255, 255) : QColor(0, 248, 0)));
        const QString prefix = current ? QStringLiteral("▶") : track;
        painter.drawText(QRect(12, y, 23, kRowHeight), Qt::AlignLeft | Qt::AlignVCenter, prefix);

        const int durationWidth = duration.isEmpty() ? 0 : metrics.horizontalAdvance(duration) + 4;
        const QRect titleRect(37, y, width() - 57 - durationWidth, kRowHeight);
        painter.save();
        painter.setClipRect(titleRect);
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);
        painter.restore();
        if (!duration.isEmpty())
            painter.drawText(QRect(width() - 20 - durationWidth, y, durationWidth, kRowHeight),
                             Qt::AlignRight | Qt::AlignVCenter, duration);
    }

    const QString totalTime = formatDuration(m_playlist->totalDuration());
    painter.setFont(QFont(QStringLiteral("DejaVu Sans Mono"), 6, QFont::Bold));
    painter.setPen(QColor("#ffe000"));
    painter.drawText(QRect(width() - 94 + 8, height() - 15, 78, 8),
                     Qt::AlignLeft | Qt::AlignVCenter, totalTime);
}

void ClassicPlaylistWindow::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (!m_windowShaded && pos.x() >= width() - 14 && pos.y() >= height() - 14) {
        m_resizing = true;
        m_resizeStartGlobal = event->globalPosition().toPoint();
        m_resizeStartSize = size();
        setCursor(Qt::SizeFDiagCursor);
        event->accept();
        return;
    }
    if (pos.y() <= (m_windowShaded ? 14 : 20)) {
        if (pos.x() >= width() - 11) {
            close();
            return;
        }
        if (pos.x() >= width() - 20 && pos.x() < width() - 11) {
            toggleWindowShade();
            return;
        }
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_dragging = true;
        emit moveStarted(event->modifiers() & Qt::ShiftModifier);
        event->accept();
        return;
    }

    if (pos.y() >= height() - 30 && pos.y() < height() - 12) {
        if (pos.x() >= 14 && pos.x() <= 35) {
            showAddMenu();
            return;
        }
        if (pos.x() >= 43 && pos.x() <= 64) {
            removeSelectedRows();
            return;
        }
        if (pos.x() >= 72 && pos.x() <= 93) {
            showSelectMenu();
            return;
        }
        if (pos.x() >= 101 && pos.x() <= 122) {
            showMiscMenu();
            return;
        }
        if (pos.x() >= width() - 44 && pos.x() <= width() - 22) {
            showListMenu();
            return;
        }
    }

    const int row = rowAt(pos);
    if (row >= 0) {
        if (!(event->modifiers() & Qt::ControlModifier))
            m_selectedRows.clear();
        if (event->modifiers() & Qt::ControlModifier && m_selectedRows.contains(row))
            m_selectedRows.remove(row);
        else
            m_selectedRows.insert(row);
        update();
    }
    QDialog::mousePressEvent(event);
}

void ClassicPlaylistWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizing && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->globalPosition().toPoint() - m_resizeStartGlobal;
        const int proposedWidth = m_resizeStartSize.width() + delta.x();
        const int proposedHeight = m_resizeStartSize.height() + delta.y();
        const int widthSteps = std::max(0, (proposedWidth - kWindowWidth + 12) / 25);
        const int heightSteps = std::max(0, (proposedHeight - kWindowHeight + 14) / 29);
        resize(kWindowWidth + widthSteps * 25, kWindowHeight + heightSteps * 29);
        event->accept();
        return;
    }
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    const QPoint pos = event->position().toPoint();
    if (!m_windowShaded && pos.x() >= width() - 14 && pos.y() >= height() - 14)
        setCursor(Qt::SizeFDiagCursor);
    else
        unsetCursor();
    QDialog::mouseMoveEvent(event);
}

void ClassicPlaylistWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizing) {
        m_resizing = false;
        unsetCursor();
        event->accept();
        return;
    }
    const bool wasDragging = m_dragging;
    m_dragging = false;
    if (wasDragging)
        emit moveFinished();
    QDialog::mouseReleaseEvent(event);
}

void ClassicPlaylistWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->position().y() <= (m_windowShaded ? 14 : 20)) {
        toggleWindowShade();
        event->accept();
        return;
    }
    const int row = rowAt(event->position().toPoint());
    if (row >= 0) {
        emit songActivated(row);
        event->accept();
        return;
    }
    QDialog::mouseDoubleClickEvent(event);
}

void ClassicPlaylistWindow::wheelEvent(QWheelEvent *event)
{
    const int visibleRows = std::max(1, (height() - kListBottomInset - kListTop) / kRowHeight);
    const int maxFirst = std::max(0, m_playlist->mediaCount() - visibleRows);
    m_firstVisibleRow = std::clamp(m_firstVisibleRow - event->angleDelta().y() / 120, 0, maxFirst);
    update();
    event->accept();
}

void ClassicPlaylistWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (!m_selectedRows.isEmpty())
            emit songActivated(*m_selectedRows.cbegin());
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void ClassicPlaylistWindow::toggleWindowShade()
{
    if (!m_windowShaded) {
        m_unshadedHeight = height();
        setFixedHeight(14);
        m_windowShaded = true;
    } else {
        setMinimumHeight(kWindowHeight);
        setMaximumHeight(QWIDGETSIZE_MAX);
        resize(width(), std::max(kWindowHeight, m_unshadedHeight));
        m_windowShaded = false;
    }
    update();
}

void ClassicPlaylistWindow::showAddMenu()
{
    QMenu menu(this);
    QAction *files = menu.addAction(tr("Add files…"));
    QAction *folder = menu.addAction(tr("Add folder…"));
    QAction *youtube = menu.addAction(tr("Add YouTube URL…"));
    const QAction *chosen = menu.exec(mapToGlobal(QPoint(22, height() - 11)));
    if (chosen == files)
        emit openFilesRequested();
    else if (chosen == folder)
        emit addFolderRequested();
    else if (chosen == youtube)
        emit addYouTubeRequested();
}

void ClassicPlaylistWindow::showSelectMenu()
{
    QMenu menu(this);
    QAction *all = menu.addAction(tr("Select all"));
    QAction *none = menu.addAction(tr("Select none"));
    QAction *invert = menu.addAction(tr("Invert selection"));
    const QAction *chosen = menu.exec(mapToGlobal(QPoint(80, height() - 11)));
    if (chosen == all) {
        m_selectedRows.clear();
        for (int row = 0; row < m_playlist->mediaCount(); ++row)
            m_selectedRows.insert(row);
    } else if (chosen == none) {
        m_selectedRows.clear();
    } else if (chosen == invert) {
        QSet<int> inverted;
        for (int row = 0; row < m_playlist->mediaCount(); ++row) {
            if (!m_selectedRows.contains(row))
                inverted.insert(row);
        }
        m_selectedRows = inverted;
    }
    update();
}

void ClassicPlaylistWindow::showMiscMenu()
{
    QMenu menu(this);
    QAction *play = menu.addAction(tr("Play selected"));
    QAction *sort = menu.addAction(tr("Sort by title"));
    QAction *reverse = menu.addAction(tr("Reverse list"));
    const QAction *chosen = menu.exec(mapToGlobal(QPoint(110, height() - 11)));
    if (chosen == play && !m_selectedRows.isEmpty())
        emit songActivated(*m_selectedRows.cbegin());
    else if (chosen == sort)
        reorderPlaylist(false);
    else if (chosen == reverse)
        reorderPlaylist(true);
}

void ClassicPlaylistWindow::showListMenu()
{
    QMenu menu(this);
    QAction *newList = menu.addAction(tr("New list"));
    QAction *saveList = menu.addAction(tr("Save list…"));
    QAction *loadList = menu.addAction(tr("Load list…"));
    const QAction *chosen = menu.exec(mapToGlobal(QPoint(width() - 35, height() - 12)));
    if (chosen == newList) {
        m_playlist->clear();
        m_selectedRows.clear();
        update();
    } else if (chosen == saveList) {
        const QString path = QFileDialog::getSaveFileName(this, tr("Save playlist"),
                                                            QDir::homePath(), tr("M3U playlist (*.m3u)"));
        if (!path.isEmpty())
            m_playlist->save(QUrl::fromLocalFile(path), "m3u");
    } else if (chosen == loadList) {
        const QString path = QFileDialog::getOpenFileName(this, tr("Load playlist"),
                                                           QDir::homePath(), tr("M3U playlist (*.m3u *.m3u8);;All files (*)"));
        if (!path.isEmpty()) {
            m_playlist->clear();
            m_selectedRows.clear();
            m_playlist->load(QUrl::fromLocalFile(path), "m3u");
        }
    }
}

void ClassicPlaylistWindow::removeSelectedRows()
{
    QList<int> rows = m_selectedRows.values();
    if (rows.isEmpty() && m_playlist->currentIndex() >= 0)
        rows.append(m_playlist->currentIndex());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows)
        m_playlist->removeMedia(row);
    m_selectedRows.clear();
    update();
}

void ClassicPlaylistWindow::reorderPlaylist(bool reverse)
{
    const int count = m_playlist->mediaCount();
    QList<int> order;
    QList<int> current;
    order.reserve(count);
    current.reserve(count);
    for (int row = 0; row < count; ++row) {
        order.append(row);
        current.append(row);
    }

    if (reverse) {
        std::reverse(order.begin(), order.end());
    } else {
        std::stable_sort(order.begin(), order.end(), [this](int left, int right) {
            return m_playlist->mediaMetadata(left).value(QMediaMetaData::Title).toString().toCaseFolded()
                 < m_playlist->mediaMetadata(right).value(QMediaMetaData::Title).toString().toCaseFolded();
        });
    }

    for (int target = 0; target < order.size(); ++target) {
        const int from = current.indexOf(order.at(target));
        if (from >= 0 && from != target) {
            m_playlist->moveMedia(from, target);
            current.move(from, target);
        }
    }
    m_selectedRows.clear();
    update();
}

void ClassicPlaylistWindow::updatePlaylist()
{
    const int visibleRows = std::max(1, (height() - kListBottomInset - kListTop) / kRowHeight);
    m_firstVisibleRow = std::clamp(m_firstVisibleRow, 0,
                                   std::max(0, m_playlist->mediaCount() - visibleRows));
    update();
}
