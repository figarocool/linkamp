#include "scrolltext.h"
#include <QPainter>
#include <algorithm>

ScrollText::ScrollText(QWidget *parent) :
    QWidget(parent), scrollPos(0)
{
    leftMargin = 0;
    setSeparator(" *** ");

    connect(&timer, &QTimer::timeout, this, &ScrollText::timer_timeout);
    timer.setInterval(80);
}

QString ScrollText::text() const
{
    return _text;
}

void ScrollText::setText(QString text)
{
    _text = text;
    updateText();
    update();
}

QString ScrollText::separator() const
{
    return _separator;
}

void ScrollText::setSeparator(QString separator)
{
    _separator = separator;
    updateText();
    update();
}

void ScrollText::updateText()
{
    timer.stop();

    singleTextWidth = static_cast<int>(_text.length()) * 5;
    scrollEnabled = (singleTextWidth > width() - leftMargin);

    const QString fullText = _text + _separator;
    wholeTextSize = QSize(std::max(5, static_cast<int>(fullText.length()) * 5), 6);

    if (scrollEnabled) {
        scrollPos = -20;
        timer.start();
    }
}

void ScrollText::renderBitmapText(QPainter &p, const QString &text, int startX, int startY)
{
    static const QPixmap fontBmp(QStringLiteral(":/assets/classic-text.png"));
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
        } else if (ch == QLatin1Char(' ')) {
            curX += 5;
            continue;
        }

        if (col >= 0 && row >= 0) {
            p.drawPixmap(curX, startY, fontBmp, col * 5, row * 6, 5, 6);
        }
        curX += 5;
    }
}

void ScrollText::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.save();
    p.setClipRect(rect());
    const int y = std::max(0, (height() - 6) / 2);

    if (scrollEnabled) {
        int x = qMin(-scrollPos, 0) + leftMargin;
        const QString fullText = _text + _separator;
        const int step = std::max(5, static_cast<int>(fullText.length()) * 5);
        while (x < width()) {
            renderBitmapText(p, fullText, x, y);
            x += step;
        }
    } else {
        renderBitmapText(p, _text, leftMargin, y);
    }
    p.restore();
}

void ScrollText::resizeEvent(QResizeEvent*)
{
    updateText();
}

void ScrollText::timer_timeout()
{
    const int totalWidth = std::max(1, wholeTextSize.width());
    scrollPos = (scrollPos + 2) % totalWidth;
    update();
}
