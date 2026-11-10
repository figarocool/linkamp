#include "classicplaylistwindow.h"

#include "util.h"
#include "winampskin.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QMenu>
#include <QMoveEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSettings>
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
constexpr int kRowHeight = 13;

static void renderBitmapTextPl(QPainter &p, const QString &text, int startX, int startY,
                               bool selected, bool current)
{
    static QPixmap normalBmp;
    static QString cachedSkinPath;
    static QPixmap selectedBmp;
    static qint64 selectedSourceCacheKey = 0;
    static QPixmap currentBmp;
    static qint64 currentSourceCacheKey = 0;
    const QString skinPath = WinampSkin::instance().filePath();
    if (normalBmp.isNull() || cachedSkinPath != skinPath) {
        QImage fontImage = WinampSkin::instance().sheet(QStringLiteral("text"));
        if (!fontImage.isNull() && !fontImage.hasAlphaChannel()) {
            fontImage = fontImage.convertToFormat(QImage::Format_ARGB32);
            const QRgb colorKey = fontImage.pixel(0, 0) & 0x00ffffff;
            for (int y = 0; y < fontImage.height(); ++y) {
                QRgb *line = reinterpret_cast<QRgb *>(fontImage.scanLine(y));
                for (int x = 0; x < fontImage.width(); ++x) {
                    if ((line[x] & 0x00ffffff) == colorKey)
                        line[x] = 0x00000000;
                }
            }
        }
        normalBmp = QPixmap::fromImage(fontImage);
        cachedSkinPath = skinPath;
        selectedSourceCacheKey = 0;
        currentSourceCacheKey = 0;
    }
    if (!normalBmp.isNull() && selectedSourceCacheKey != normalBmp.cacheKey()) {
        QImage img = normalBmp.toImage().convertToFormat(QImage::Format_RGB32);
        for (int y = 0; y < img.height(); ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                if (qGreen(line[x]) > 50 || qRed(line[x]) > 50 || qBlue(line[x]) > 50) {
                    line[x] = qRgb(0, 0, 0); // glyph is black
                } else {
                    line[x] = qRgb(0, 190, 0); // background is green (#00be00)
                }
            }
        }
        selectedBmp = QPixmap::fromImage(img);
        selectedSourceCacheKey = normalBmp.cacheKey();
    }
    if (!normalBmp.isNull() && currentSourceCacheKey != normalBmp.cacheKey()) {
        QImage img = normalBmp.toImage().convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                if (qGreen(line[x]) > 50)
                    line[x] = qRgba(255, 255, 255, 255);
                else
                    line[x] = qRgba(0, 0, 0, 0);
            }
        }
        currentBmp = QPixmap::fromImage(img);
        currentSourceCacheKey = normalBmp.cacheKey();
    }
    const QPixmap &fontBmp = selected ? selectedBmp : current ? currentBmp : normalBmp;
    if (fontBmp.isNull()) return;

    int curX = startX;
    for (int i = 0; i < text.length(); ++i) {
        const QChar ch = text[i];
        int col = -1;
        int row = -1;

        if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) {
            row = 0;
            col = ch.unicode() - 'A';
        } else if (ch >= QLatin1Char('a') && ch <= QLatin1Char('z')) {
            row = 0;
            col = ch.unicode() - 'a';
        } else if (ch == QLatin1Char('"')) {
            row = 0; col = 26;
        } else if (ch == QLatin1Char('@')) {
            row = 0; col = 27;
        } else if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
            row = 1;
            col = ch.unicode() - '0';
        } else if (ch == QLatin1Char('.')) {
            row = 1; col = 11;
        } else if (ch == QLatin1Char(':')) {
            row = 1; col = 12;
        } else if (ch == QLatin1Char('(')) {
            row = 1; col = 13;
        } else if (ch == QLatin1Char(')')) {
            row = 1; col = 14;
        } else if (ch == QLatin1Char('-')) {
            row = 1; col = 15;
        } else if (ch == QLatin1Char('\'') || ch == QLatin1Char('`')) {
            row = 1; col = 16;
        } else if (ch == QLatin1Char('!')) {
            row = 1; col = 17;
        } else if (ch == QLatin1Char('_')) {
            row = 1; col = 18;
        } else if (ch == QLatin1Char('+')) {
            row = 1; col = 19;
        } else if (ch == QLatin1Char('\\')) {
            row = 1; col = 20;
        } else if (ch == QLatin1Char('/')) {
            row = 1; col = 21;
        } else if (ch == QLatin1Char('[')) {
            row = 1; col = 22;
        } else if (ch == QLatin1Char(']')) {
            row = 1; col = 23;
        } else if (ch == QLatin1Char('^')) {
            row = 1; col = 24;
        } else if (ch == QLatin1Char('&')) {
            row = 1; col = 25;
        } else if (ch == QLatin1Char('%')) {
            row = 1; col = 26;
        } else if (ch == QLatin1Char(',')) {
            row = 1; col = 27;
        } else if (ch == QLatin1Char('=')) {
            row = 1; col = 28;
        } else if (ch == QLatin1Char('$')) {
            row = 1; col = 29;
        } else if (ch == QLatin1Char('#')) {
            row = 1; col = 30;
        } else if (ch == QLatin1Char('?')) {
            row = 2; col = 3;
        } else if (ch == QLatin1Char(' ')) {
            if (selected) {
                p.fillRect(curX, startY, 5, 6, QColor(0, 190, 0));
            }
            curX += 5;
            continue;
        }

        if (col >= 0 && row >= 0) {
            p.drawPixmap(curX, startY, fontBmp, col * 5, row * 6, 5, 6);
        } else if (selected) {
            p.fillRect(curX, startY, 5, 6, QColor(0, 190, 0));
        }
        curX += 5;
    }
}

static void renderPlaylistTitle(QPainter &p, const QPixmap &sheet, int titleY, int slotX)
{
    // PLEDIT.BMP embeds the Winamp product name in both its active and
    // inactive title strips. Reserve the same 100x20 logo slot for Linkamp
    // and draw it with the skin's own 5x6 bitmap font.
    const QImage sheetImage = sheet.toImage();
    const QColor background = sheetImage.pixelColor(30, titleY + 8);
    p.fillRect(QRect(slotX, 0, 100, 20), background);

    const QString title = QStringLiteral("LINKAMP PLAYLIST");
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(10);
    font.setBold(true);
    QFontMetrics metrics(font);
    while (metrics.horizontalAdvance(title) > 96 && font.pixelSize() > 8) {
        font.setPixelSize(font.pixelSize() - 1);
        metrics = QFontMetrics(font);
    }
    const int startX = slotX + (100 - metrics.horizontalAdvance(title)) / 2;
    const QColor color = titleY == 0 ? QColor(255, 255, 255) : QColor(144, 144, 156);
    p.setFont(font);
    p.setPen(color);
    p.drawText(startX, 14, title);
}

static void renderBuiltinInactivePlaylistTitle(QPainter &p, const QPixmap &sheet, int slotX)
{
    // The bundled PLEDIT sheet already has Linkamp's pixel logo in its active
    // strip. Reuse that exact glyph mask on the inactive strip, replacing the
    // original Winamp lettering while keeping the inactive bar's colors.
    const QImage image = sheet.toImage();
    // The bundled inactive PLEDIT title strip has a stray cyan pixel column
    // at source x=64. Restore the strip's original shading from its neighbors
    // before drawing the Linkamp title over it.
    const int strayColumn = 64;
    for (int y = 0; y < 20; ++y) {
        const QColor left = image.pixelColor(strayColumn - 1, 21 + y);
        const QColor right = image.pixelColor(strayColumn + 1, 21 + y);
        p.fillRect(QRect(slotX + strayColumn - 26, y, 1, 1),
                   QColor((left.red() + right.red()) / 2,
                          (left.green() + right.green()) / 2,
                          (left.blue() + right.blue()) / 2));
    }
    const QColor background = image.pixelColor(26 + 52, 21 + 9);
    constexpr int sourceLeft = 13;
    constexpr int sourceWidth = 75;
    constexpr int targetLeft = 8;
    constexpr int targetWidth = 84;
    p.fillRect(QRect(slotX + targetLeft, 5, targetWidth, 7), background);

    for (int y = 5; y <= 10; ++y) {
        for (int x = sourceLeft; x < sourceLeft + sourceWidth; ++x) {
            const QColor source = image.pixelColor(26 + x, y);
            const int luminance = (source.red() + source.green() + source.blue()) / 3;
            const int alpha = std::clamp((luminance - 100) * 255 / (250 - 100), 0, 255);
            if (alpha > 0) {
                const int left = targetLeft + (x - sourceLeft) * targetWidth / sourceWidth;
                const int right = targetLeft + (x - sourceLeft + 1) * targetWidth / sourceWidth;
                p.fillRect(QRect(slotX + left, y, std::max(1, right - left), 1),
                           QColor(144, 144, 156, alpha));
            }
        }
    }
}
}

ClassicPlaylistWindow::ClassicPlaylistWindow(QMediaPlaylist *playlist, QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_playlist(playlist)
{
    setWindowTitle(QStringLiteral("Linkamp Playlist"));
    setMinimumSize(kWindowWidth, kWindowHeight);
    const QSettings settings;
    const QSize savedSize(
        settings.value(QStringLiteral("playlist/width"), kWindowWidth).toInt(),
        settings.value(QStringLiteral("playlist/height"), kWindowHeight).toInt());
    resize(qMax(kWindowWidth, savedSize.width()),
           qMax(kWindowHeight, savedSize.height()));
    const QString savedPositionKey = QStringLiteral("windows/playlist/position");
    if (settings.contains(savedPositionKey)) {
        move(settings.value(savedPositionKey).toPoint());
        setProperty("savedWindowPosition", true);
    }
    m_unshadedHeight = height();
    setMouseTracking(true);

    connect(m_playlist, &QMediaPlaylist::mediaInserted, this, &ClassicPlaylistWindow::updatePlaylist);
    connect(m_playlist, &QMediaPlaylist::mediaRemoved, this, &ClassicPlaylistWindow::updatePlaylist);
    connect(m_playlist, &QMediaPlaylist::mediaChanged, this, &ClassicPlaylistWindow::updatePlaylist);
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, &ClassicPlaylistWindow::handleCurrentIndexChanged);

    if (m_playlist->currentIndex() >= 0 && m_playlist->currentIndex() < m_playlist->mediaCount())
        m_selectedRows.insert(m_playlist->currentIndex());
}

void ClassicPlaylistWindow::setPosition(qint64 position)
{
    m_position = position;
    update();
}

void ClassicPlaylistWindow::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (!m_windowShaded) {
        m_unshadedHeight = height();
        QSettings settings;
        settings.setValue(QStringLiteral("playlist/width"), width());
        settings.setValue(QStringLiteral("playlist/height"), height());
    }
}

void ClassicPlaylistWindow::moveEvent(QMoveEvent *event)
{
    QDialog::moveEvent(event);
    if (isVisible())
        QSettings().setValue(QStringLiteral("windows/playlist/position"),
                             frameGeometry().topLeft());
}

void ClassicPlaylistWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        update();
    }
    QDialog::changeEvent(event);
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
    const QPixmap sprite = QPixmap::fromImage(
        WinampSkin::instance().sheet(QStringLiteral("pledit")));
    if (sprite.isNull()) {
        painter.fillRect(rect(), QColor(12, 14, 24));
        return;
    }

    auto blit = [&painter, &sprite](int x, int y, int width, int height,
                                  int sourceX, int sourceY, int sourceWidth, int sourceHeight) {
        painter.drawPixmap(QRect(x, y, width, height), sprite,
                           QRect(sourceX, sourceY, sourceWidth, sourceHeight));
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
        return;
    }

    const int titleY = (window() && window()->isActiveWindow()) ? 0 : 21;
    int x = 0;
    int tiles = (width() - 25 - 25 - 100) / 25;
    blit(x, 0, 25, 20, 0, titleY, 25, 20);
    x += 25;
    if (tiles & 1) {
        blit(x, 0, 12, 20, 127, titleY, 12, 20);
        x += 12;
    }
    tiles /= 2;
    while (tiles-- > 0) {
        blit(x, 0, 25, 20, 127, titleY, 25, 20);
        x += 25;
    }
    const int logoSlotX = x;
    blit(x, 0, 100, 20, 26, titleY, 100, 20);
    x += 100;
    tiles = (width() - 25 - 25 - 100) / 25;
    if (tiles & 1) {
        blit(x, 0, 13, 20, 127, titleY, 13, 20);
        x += 13;
    }
    tiles /= 2;
    while (tiles-- > 0) {
        blit(x, 0, 25, 20, 127, titleY, 25, 20);
        x += 25;
    }
    const int remainder = (width() - 25 - 25 - 100) % 25;
    if (remainder > 0) {
        blit(x, 0, remainder, 20, 127, titleY, remainder, 20);
        x += remainder;
    }
    blit(x, 0, 25, 20, 153, titleY, 25, 20);
    blit(width() - 11, 3, 9, 9, 167, 3, 9, 9);
    blit(width() - 20, 3, 9, 9, 158, 3, 9, 9);
    Q_UNUSED(logoSlotX);
    const PlaylistSkinTheme &plTheme = WinampSkin::instance().playlistTheme();
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
        blit(0, borderY, 12, borderRemainder, 0, 42, 12, borderRemainder);
        blit(width() - 20, borderY, 5, borderRemainder, 31, 42, 5, borderRemainder);
        blit(width() - 7, borderY, 7, borderRemainder, 44, 42, 7, borderRemainder);
    }
    painter.fillRect(QRect(12, 20, width() - 32, trackHeight), plTheme.normalBackground);

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
        blit(bottomX, bottomY, middleRemainder, 38, 179, 0, middleRemainder, 38);
        bottomX += middleRemainder;
    }
    if (extraPanel) {
        blit(bottomX, bottomY, 75, 38, 205, 0, 75, 38);
        bottomX += 75;
    }
    blit(bottomX, bottomY, width() - bottomX, 38, 126, 72, 150, 38);

    const int thumbHeight = 18;
    const int visibleRows = std::max(1, (height() - 38 - 20 - 2) / kRowHeight);
    const int maxFirstRow = std::max(0, m_playlist->mediaCount() - visibleRows);
    const int thumbTravel = std::max(0, trackHeight - thumbHeight - 3);
    const int thumbY = maxFirstRow == 0
        ? 0 : (thumbTravel * m_firstVisibleRow) / maxFirstRow;
    for (int y = 0; y < trackHeight; y += 29) {
        const int height = std::min(29, trackHeight - y);
        blit(width() - 15, 20 + y, 8, height, 36, 42, 8, height);
    }
    blit(width() - 15, 20 + thumbY, 8, thumbHeight, m_draggingScrollbar ? 61 : 52, 53, 8, 18);

    const int end = std::min(m_playlist->mediaCount(), m_firstVisibleRow + visibleRows);
    // Keep the small playlist text neutral white on Linux. Subpixel font
    // antialiasing tints the one-pixel strokes on the dark skin; grayscale
    // antialiasing keeps smooth edges without the colored fringe.
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    for (int row = m_firstVisibleRow; row < end; ++row) {
        const int y = kListTop + (row - m_firstVisibleRow) * kRowHeight;
        const QRect rowRect(12, y, width() - 32, kRowHeight);
        const bool selected = m_selectedRows.contains(row);
        const bool current = m_playlist->currentIndex() == row;
        if (selected || current) {
            painter.fillRect(rowRect, plTheme.selectedBackground);
        } else {
            painter.fillRect(rowRect, plTheme.normalBackground);
        }

        const QMediaMetaData metadata = m_playlist->mediaMetadata(row);
        QString title = repairUtf8Mojibake(
            metadata.value(QMediaMetaData::Title).toString());
        const QFileInfo mediaFile(m_playlist->media(row).fileName());
        const QString metadataFileName = QFileInfo(title).fileName();
        if (title.isEmpty()
            || metadataFileName.compare(mediaFile.fileName(), Qt::CaseInsensitive) == 0) {
            title = mediaFile.completeBaseName();
        }
        const QString artist = repairUtf8Mojibake(
            metadata.value(QMediaMetaData::AlbumArtist).toString());
        const QString author = artist.isEmpty()
            ? repairUtf8Mojibake(metadata.value(QMediaMetaData::Author).toString())
            : artist;
        if (!author.isEmpty())
            title = author + QStringLiteral(" - ") + title;

        const QString track = QStringLiteral("%1. ").arg(row + 1);
        const QString fullTitle = track + title;

        QString duration;
        const qint64 durationMs = metadata.value(QMediaMetaData::Duration).toLongLong();
        if (durationMs > 0)
            duration = formatDuration(durationMs);

        // Winamp's playlist rows use a normal UI font; TEXT.BMP is reserved
        // for the small LCD readouts and drawing rows with it makes them look
        // like a tiny green terminal instead of the original playlist.
        QFont rowFont(plTheme.fontFamily);
        rowFont.setPixelSize(13);
        rowFont.setStyleStrategy(QFont::StyleStrategy(
            QFont::PreferAntialias | QFont::NoSubpixelAntialias));
        painter.setFont(rowFont);
        painter.setPen(current ? plTheme.currentText : plTheme.normalText);
        const QFontMetrics metrics(rowFont);
        const int durationWidth = duration.isEmpty() ? 0 : metrics.horizontalAdvance(duration) + 6;
        const int maxTextWidth = (width() - 36) - durationWidth;
        const int textBaseline = y + 11;
        const QString elidedTitle = metrics.elidedText(fullTitle, Qt::ElideRight, maxTextWidth);
        painter.save();
        painter.setClipRect(QRect(14, y, maxTextWidth, kRowHeight));
        painter.drawText(14, textBaseline, elidedTitle);
        painter.restore();

        if (!duration.isEmpty()) {
            const int durX = (width() - 22) - metrics.horizontalAdvance(duration);
            painter.drawText(durX, textBaseline, duration);
        }
    }

    const QString currentTime = formatDuration(m_position);
    const QString totalTime = formatDuration(m_playlist->totalDuration());
    const QString comboTime = (m_playlist->mediaCount() > 0 && m_playlist->totalDuration() > 0)
        ? (currentTime + QLatin1Char('/') + totalTime)
        : QStringLiteral("0:00/?");

    // Top LCD box (track time / total time): left-aligned starting at width() - 143 (132), height() - 28 (88)
    renderBitmapTextPl(painter, comboTime, width() - 143, height() - 28, false, false);

    // Bottom LCD box (current track time): only rendered when playing (m_position > 0)
    if (m_position > 0) {
        const int botLcdX = (width() - 86);
        renderBitmapTextPl(painter, currentTime, botLcdX, height() - 15, false, false);
    }
}

void ClassicPlaylistWindow::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (event->button() == Qt::RightButton) {
        const int row = rowAt(pos);
        if (row >= 0) {
            m_selectedRows.clear();
            m_selectedRows.insert(row);
            update();

            const QUrl url = m_playlist->media(row);
            const bool youtube = isYouTubeUrl(url);
            const QString copyText = youtube ? url.toString(QUrl::FullyEncoded)
                : (url.isLocalFile() ? url.toLocalFile() : url.toString());
            if (!copyText.isEmpty()) {
                QMenu menu(this);
                QAction *copy = menu.addAction(youtube ? tr("Copy YouTube link")
                                                      : tr("Copy path"));
                if (menu.exec(event->globalPosition().toPoint()) == copy)
                    QApplication::clipboard()->setText(copyText);
            }
            event->accept();
            return;
        }
    }

    if (!m_windowShaded && event->button() == Qt::LeftButton
        && pos.x() >= width() - 16 && pos.x() < width() - 7
        && pos.y() >= 20 && pos.y() < height() - 38) {
        const int visibleRows = std::max(1, (height() - 38 - 20 - 2) / kRowHeight);
        const int maxFirstRow = std::max(0, m_playlist->mediaCount() - visibleRows);
        const int trackHeight = height() - 20 - 38;
        const int thumbHeight = 18;
        const int thumbTravel = std::max(0, trackHeight - thumbHeight - 3);
        if (maxFirstRow > 0 && thumbTravel > 0) {
            const int thumbTop = 20 + (thumbTravel * m_firstVisibleRow) / maxFirstRow;
            if (pos.y() >= thumbTop && pos.y() < thumbTop + thumbHeight) {
                m_scrollbarDragOffset = pos.y() - thumbTop;
            } else {
                m_scrollbarDragOffset = thumbHeight / 2;
                const int thumbPosition = std::clamp(
                    pos.y() - 20 - m_scrollbarDragOffset, 0, thumbTravel);
                m_firstVisibleRow = std::clamp(
                    (thumbPosition * maxFirstRow + thumbTravel / 2) / thumbTravel,
                    0, maxFirstRow);
            }
            m_draggingScrollbar = true;
            update();
        }
        event->accept();
        return;
    }
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
        if (event->modifiers() & Qt::ControlModifier && m_selectedRows.contains(row)) {
            m_selectedRows.remove(row);
        } else {
            m_selectedRows.insert(row);
            if (!(event->modifiers() & Qt::ControlModifier)) {
                m_draggingRow = true;
                m_draggedRow = row;
            }
        }
        update();
    }
    QDialog::mousePressEvent(event);
}

void ClassicPlaylistWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingRow && (event->buttons() & Qt::LeftButton)) {
        const int count = m_playlist->mediaCount();
        if (count > 1 && m_draggedRow >= 0 && m_draggedRow < count) {
            const QPoint pos = event->position().toPoint();
            const int visibleRows = std::max(1, (height() - kListBottomInset - kListTop) / kRowHeight);
            const int maxFirstRow = std::max(0, count - visibleRows);
            if (pos.y() < kListTop && m_firstVisibleRow > 0) {
                --m_firstVisibleRow;
            } else if (pos.y() >= kListTop + visibleRows * kRowHeight && m_firstVisibleRow < maxFirstRow) {
                ++m_firstVisibleRow;
            }
            const int relRow = (pos.y() - kListTop) / kRowHeight;
            const int targetRow = std::clamp(m_firstVisibleRow + relRow, 0, count - 1);
            if (targetRow != m_draggedRow) {
                m_playlist->moveMedia(m_draggedRow, targetRow);
                m_selectedRows.clear();
                m_selectedRows.insert(targetRow);
                m_draggedRow = targetRow;
                update();
            }
        }
        event->accept();
        return;
    }
    if (m_draggingScrollbar && (event->buttons() & Qt::LeftButton)) {
        const int visibleRows = std::max(1, (height() - 38 - 20 - 2) / kRowHeight);
        const int maxFirstRow = std::max(0, m_playlist->mediaCount() - visibleRows);
        const int trackHeight = height() - 20 - 38;
        const int thumbTravel = std::max(0, trackHeight - 18 - 3);
        if (maxFirstRow > 0 && thumbTravel > 0) {
            const int thumbPosition = std::clamp(
                event->position().toPoint().y() - 20 - m_scrollbarDragOffset,
                0, thumbTravel);
            m_firstVisibleRow = std::clamp(
                (thumbPosition * maxFirstRow + thumbTravel / 2) / thumbTravel,
                0, maxFirstRow);
            update();
        }
        event->accept();
        return;
    }
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
    if (m_draggingRow && event->button() == Qt::LeftButton) {
        m_draggingRow = false;
        m_draggedRow = -1;
        event->accept();
        return;
    }
    if (m_draggingScrollbar && event->button() == Qt::LeftButton) {
        m_draggingScrollbar = false;
        event->accept();
        return;
    }
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

void ClassicPlaylistWindow::handleCurrentIndexChanged(int index)
{
    m_selectedRows.clear();
    if (index >= 0 && index < m_playlist->mediaCount()) {
        m_selectedRows.insert(index);
        const int visibleRows = std::max(1, (height() - kListBottomInset - kListTop) / kRowHeight);
        if (index < m_firstVisibleRow)
            m_firstVisibleRow = index;
        else if (index >= m_firstVisibleRow + visibleRows)
            m_firstVisibleRow = std::max(0, index - visibleRows + 1);
    }
    updatePlaylist();
}
