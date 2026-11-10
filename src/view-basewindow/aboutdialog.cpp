#include "aboutdialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFrame>
#include <QColor>
#include <QGuiApplication>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcess>
#include <QPen>
#include <QRadialGradient>
#include <QStackedWidget>
#include <QScreen>
#include <QStringList>
#include <QStyle>
#include <QTabBar>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

#ifndef LINKAMP_VERSION
#define LINKAMP_VERSION "0.0.0"
#endif

namespace {
constexpr auto kProjectUrl = "https://github.com/figarocool/linkamp";
constexpr auto kOriginalProjectUrl = "https://github.com/Rodmg/linamp";

class LinkampLogoScene final : public QWidget
{
public:
    explicit LinkampLogoScene(QWidget *parent = nullptr)
        : QWidget(parent), m_logo(QStringLiteral(":/assets/linkamp-mark.png"))
    {
        setMinimumHeight(230);
        m_clock.start();
        auto *timer = new QTimer(this);
        timer->setInterval(40);
        connect(timer, &QTimer::timeout, this, [this] { update(); });
        timer->start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setClipRect(rect());

        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0.0, QColor(4, 9, 23));
        background.setColorAt(0.52, QColor(18, 27, 54));
        background.setColorAt(1.0, QColor(3, 7, 19));
        painter.fillRect(rect(), background);

        const qreal phase = m_clock.elapsed() / 1000.0;
        const QPointF center(width() / 2.0, height() * 0.49);
        QRadialGradient glow(center, width() * 0.48);
        glow.setColorAt(0.0, QColor(105, 174, 255, 78));
        glow.setColorAt(0.35, QColor(40, 104, 207, 42));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(rect(), glow);

        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Screen);
        for (int i = 0; i < 13; ++i) {
            const qreal spread = (i - 6) * 0.13;
            const qreal sway = std::sin(phase * 0.7 + i * 0.8) * 0.08;
            QPainterPath streak;
            streak.moveTo(center.x() - width() * 0.62, center.y() + height() * spread);
            streak.cubicTo(center.x() - width() * 0.18, center.y() + height() * (spread + sway),
                           center.x() + width() * 0.2, center.y() + height() * (spread - sway),
                           center.x() + width() * 0.62, center.y() + height() * spread);
            const int alpha = 14 + (i % 4) * 5;
            painter.setPen(QPen(QColor(88, 168, 255, alpha), (i % 3 == 0) ? 2.2 : 1.0));
            painter.drawPath(streak);
        }
        painter.restore();

        if (m_logo.isNull())
            return;

        const qreal size = qMin(width() * 0.42, height() * 0.78);
        const qreal spin = phase * 0.62;
        const qreal faceWidth = 0.36 + 0.64 * qAbs(std::cos(spin));
        const qreal lean = std::sin(spin) * 0.22;

        painter.save();
        painter.translate(center);
        painter.shear(lean, 0.0);
        painter.scale(faceWidth, 1.0);
        const QRectF logoRect(-size / 2.0, -size / 2.0, size, size);

        // Layered copies give the existing Linkamp mark a moving, extruded edge.
        for (int depth = 14; depth >= 1; --depth) {
            painter.setOpacity(0.035 + (14 - depth) * 0.018);
            const qreal offset = depth * 0.72;
            painter.drawPixmap(logoRect.translated(offset, offset * 0.34), m_logo, m_logo.rect());
        }
        painter.setOpacity(1.0);
        painter.drawPixmap(logoRect, m_logo, m_logo.rect());
        painter.restore();

        QLinearGradient reflection(0, height() - 18, 0, height());
        reflection.setColorAt(0.0, QColor(106, 173, 255, 0));
        reflection.setColorAt(1.0, QColor(106, 173, 255, 24));
        painter.fillRect(QRect(0, height() - 18, width(), 18), reflection);
    }

private:
    QPixmap m_logo;
    QElapsedTimer m_clock;
};

class CreditsScene final : public QWidget
{
public:
    explicit CreditsScene(QWidget *parent = nullptr) : QWidget(parent)
    {
        m_clock.start();
        auto *timer = new QTimer(this);
        timer->setInterval(40);
        connect(timer, &QTimer::timeout, this, [this] { update(); });
        timer->start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setClipRect(rect());
        p.fillRect(rect(), QColor(15, 5, 1));

        // A moving, faceted perspective tunnel recreates the classic 3D credits
        // visualization without relying on a particular external visualizer.
        const qreal time = m_clock.elapsed() / 1000.0;
        const QPointF vanishing(width() * (0.5 + 0.07 * std::sin(time * 0.32)),
                               height() * (0.50 + 0.06 * std::cos(time * 0.27)));
        const int rings = 9;
        const int facets = 16;
        for (int ring = rings; ring >= 1; --ring) {
            const qreal radius = (0.16 + ring * 0.15) * qMax(width(), height());
            const qreal nextRadius = radius + qMax(width(), height()) * 0.14;
            for (int i = 0; i < facets; ++i) {
                const qreal a0 = (i * 2.0 * M_PI / facets) + time * 0.17;
                const qreal a1 = ((i + 1) * 2.0 * M_PI / facets) + time * 0.17;
                QPolygonF face;
                face << vanishing + QPointF(std::cos(a0) * radius, std::sin(a0) * radius)
                     << vanishing + QPointF(std::cos(a1) * radius, std::sin(a1) * radius)
                     << vanishing + QPointF(std::cos(a1) * nextRadius, std::sin(a1) * nextRadius)
                     << vanishing + QPointF(std::cos(a0) * nextRadius, std::sin(a0) * nextRadius);
                const int wave = int((std::sin(time * 0.8 + i * 0.7 + ring) + 1.0) * 25);
                p.setPen(QPen(QColor(255, 174 + wave, 39, 110), 1));
                p.setBrush(QColor(105 + wave * 2, 30 + wave, 2, 225));
                p.drawPolygon(face);
            }
        }

        const QStringList lines = {
            QStringLiteral("LINKAMP"),
            QStringLiteral("Created by Stefano Basile"),
            QStringLiteral("Based on Linamp by Rodrigo Mendez"),
            QStringLiteral("Inspired by Winamp Classic"),
            QStringLiteral("Built with Qt 6 and TagLib")
        };
        QFont font = p.font();
        font.setFamily(QStringLiteral("DejaVu Sans"));
        font.setBold(true);
        font.setPixelSize(qBound(13, width() / 24, 20));
        p.setFont(font);
        const int lineHeight = font.pixelSize() + 5;
        const qreal scroll = std::fmod(time * 17.0, (lines.size() + 2) * lineHeight);
        for (int i = 0; i < lines.size(); ++i) {
            const qreal y = height() + i * lineHeight - scroll;
            if (y < -lineHeight || y > height() + lineHeight)
                continue;
            const QString text = lines.at(i);
            const qreal x = (width() - QFontMetricsF(font).horizontalAdvance(text)) / 2.0;
            p.setPen(QColor(30, 8, 0, 230));
            for (int depth = 5; depth >= 1; --depth)
                p.drawText(QPointF(x + depth, y + depth), text);
            p.setPen(QColor(255, 249, 218));
            p.drawText(QPointF(x, y), text);
        }
    }

private:
    QElapsedTimer m_clock;
};

QTextBrowser *makeTextPage(const QString &html, QWidget *parent)
{
    auto *browser = new QTextBrowser(parent);
    browser->setOpenLinks(false);
    browser->setOpenExternalLinks(false);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setStyleSheet(QStringLiteral(
        "QTextBrowser { background:#ededee; color:#181820; padding:12px; "
        "selection-background-color:#3a6fae; }"));
    browser->setHtml(html);
    QObject::connect(browser, &QTextBrowser::anchorClicked, browser,
                     [](const QUrl &url) { QDesktopServices::openUrl(url); });
    return browser;
}

QString historyHtml()
{
    QProcess git;
#ifdef LINKAMP_SOURCE_DIR
    git.start(QStringLiteral("git"), {
        QStringLiteral("-C"), QStringLiteral(LINKAMP_SOURCE_DIR), QStringLiteral("log"),
        QStringLiteral("-12"), QStringLiteral("--format=%h%x09%ad%x09%s"),
        QStringLiteral("--date=short")
    });
    if (git.waitForStarted(800) && git.waitForFinished(1800) && git.exitCode() == 0) {
        const QString output = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            QString html = QStringLiteral(
                "<h2>Version history</h2><p>Recent changes from the project Git history.</p>"
                "<table cellspacing='5' cellpadding='2'>");
            const QStringList entries = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &entry : entries) {
                const QStringList columns = entry.split(QLatin1Char('\t'));
                if (columns.size() < 3)
                    continue;
                const QString hash = columns.at(0).toHtmlEscaped();
                const QString date = columns.at(1).toHtmlEscaped();
                const QString subject = columns.mid(2).join(QLatin1Char('\t')).toHtmlEscaped();
                const QString commitUrl = QStringLiteral("%1/commit/%2")
                    .arg(QString::fromLatin1(kProjectUrl), hash);
                html += QStringLiteral(
                    "<tr><td><a href='%1'>%2</a></td><td>%3</td><td>%4</td></tr>")
                    .arg(commitUrl.toHtmlEscaped(), hash, date, subject);
            }
            html += QStringLiteral("</table><p><a href='%1/commits/main'>Open full history on GitHub</a></p>")
                        .arg(QString::fromLatin1(kProjectUrl));
            return html;
        }
    }
#endif
    return QStringLiteral(
        "<h2>Version history</h2><p>Commit history is available in the project repository.</p>"
        "<p><a href='%1/commits/main'>Open Linkamp history on GitHub</a></p>")
        .arg(QString::fromLatin1(kProjectUrl));
}
}

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle(tr("About Linkamp"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/linkamp-mark.png")));
    setFixedSize(480, 408);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        move(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(),
                                 screen->availableGeometry()).topLeft());
    }
    setStyleSheet(QStringLiteral(
        "QDialog { background:#c9c9cd; }"
        "QTabBar::tab { color:#17171d; background:#d6d6d8; padding:5px 10px; "
        "border-right:1px solid #898991; border-bottom:1px solid #777780; min-width:75px; }"
        "QTabBar::tab:selected { background:#f0f0f1; border-bottom:1px solid #f0f0f1; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(0);

    auto *tabs = new QTabBar(this);
    tabs->setExpanding(true);
    tabs->setUsesScrollButtons(false);
    tabs->addTab(tr("About Linkamp"));
    tabs->addTab(tr("Credits"));
    tabs->addTab(tr("Keyboard shortcuts"));
    tabs->addTab(tr("Version history"));
    layout->addWidget(tabs);

    auto *pages = new QStackedWidget(this);
    pages->setStyleSheet(QStringLiteral(
        "QStackedWidget { background:#ededee; border:1px solid #777780; "
        "border-top:0; }"));
    layout->addWidget(pages, 1);

    auto *aboutPage = new QWidget(pages);
    auto *aboutLayout = new QVBoxLayout(aboutPage);
    aboutLayout->setContentsMargins(5, 0, 5, 8);
    aboutLayout->setSpacing(5);
    auto *logoScene = new LinkampLogoScene(aboutPage);
    aboutLayout->addWidget(logoScene, 1);
    auto *aboutText = new QLabel(
        tr("Linkamp %1\nCopyright © 2023–2026 Stefano Basile\n"
           "Inspired by Winamp Classic. Visit <a href='%2'>GitHub</a> for updates.")
            .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(kProjectUrl)),
        aboutPage);
    aboutText->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    aboutText->setTextFormat(Qt::RichText);
    aboutText->setStyleSheet(QStringLiteral("QLabel { color:#202028; } a { color:#183d9a; }"));
    aboutLayout->addWidget(aboutText);
    connect(aboutText, &QLabel::linkActivated, this,
            [](const QString &link) { QDesktopServices::openUrl(QUrl(link)); });
    pages->addWidget(aboutPage);

    auto *creditsPage = new QWidget(pages);
    auto *creditsLayout = new QVBoxLayout(creditsPage);
    creditsLayout->setContentsMargins(0, 0, 0, 0);
    creditsLayout->setSpacing(0);
    creditsLayout->addWidget(new CreditsScene(creditsPage), 1);
    auto *creditsLinks = new QLabel(
        tr("<a href='%1'>Linamp by Rodrigo Méndez</a> · "
           "<a href='%2'>Linkamp source</a>")
            .arg(QString::fromLatin1(kOriginalProjectUrl), QString::fromLatin1(kProjectUrl)),
        creditsPage);
    creditsLinks->setAlignment(Qt::AlignCenter);
    creditsLinks->setTextFormat(Qt::RichText);
    creditsLinks->setStyleSheet(QStringLiteral("QLabel { background:#d7d7d9; padding:4px; }"));
    connect(creditsLinks, &QLabel::linkActivated, this,
            [](const QString &link) { QDesktopServices::openUrl(QUrl(link)); });
    creditsLayout->addWidget(creditsLinks);
    pages->addWidget(creditsPage);

    const QString shortcuts = QStringLiteral(
        "<h2>Keyboard shortcuts</h2>"
        "<p><b>Playlist</b></p>"
        "<table cellspacing='5' cellpadding='2'>"
        "<tr><td><b>Enter</b></td><td>Play the selected track</td></tr>"
        "</table>"
        "<p><b>MilkDrop</b></p>"
        "<table cellspacing='5' cellpadding='2'>"
        "<tr><td><b>Space / N / →</b></td><td>Next preset</td></tr>"
        "<tr><td><b>P / Backspace / ←</b></td><td>Previous preset</td></tr>"
        "<tr><td><b>R</b></td><td>Random preset</td></tr>"
        "<tr><td><b>L</b></td><td>Show preset information</td></tr>"
        "<tr><td><b>F / Enter</b></td><td>Toggle full screen</td></tr>"
        "<tr><td><b>Esc</b></td><td>Leave full screen</td></tr>"
        "</table>"
        "<p><b>Video</b></p>"
        "<table cellspacing='5' cellpadding='2'>"
        "<tr><td><b>F / Enter</b></td><td>Toggle full screen</td></tr>"
        "<tr><td><b>Esc</b></td><td>Leave full screen</td></tr>"
        "</table><p>Transport controls do not currently have global keyboard shortcuts.</p>");
    pages->addWidget(makeTextPage(shortcuts, pages));
    pages->addWidget(makeTextPage(historyHtml(), pages));

    connect(tabs, &QTabBar::currentChanged, pages, &QStackedWidget::setCurrentIndex);
}
