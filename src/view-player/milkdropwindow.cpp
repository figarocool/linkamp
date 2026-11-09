#include "milkdropwindow.h"

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
#include <QPainterPath>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "fft.h"

namespace {
constexpr int kMinWindowWidth = 275;
constexpr int kMinWindowHeight = 116;
constexpr int kTitleBarHeight = 20;
constexpr int kBottomBarHeight = 14;
constexpr int kLeftBorderWidth = 11;
constexpr int kRightBorderWidth = 8;
constexpr int kPresetCount = 8;

const char *const kPresetNames[kPresetCount] = {
    "Geiss - Cosmic Dust II",
    "Rovastar - Parallel Universe",
    "Unchained - Beat Demo (Remix)",
    "Martin - Plasma Tunnel",
    "Krash & Rovastar - Kaleidoscope Dreams",
    "Zylot - Starfield Warp Drive",
    "Flexi - Predator Prey Spirals",
    "Geiss & Suksma - Feedback Matrix"
};

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

inline QRgb sampleBilinear(const QRgb * __restrict srcBits, int w, int h, float u, float v)
{
    const float fx = std::clamp(u * float(w - 1), 0.0f, float(w - 1.001f));
    const float fy = std::clamp(v * float(h - 1), 0.0f, float(h - 1.001f));
    const int x0 = int(fx);
    const int y0 = int(fy);
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const float tx = fx - float(x0);
    const float ty = fy - float(y0);

    const QRgb c00 = srcBits[y0 * w + x0];
    const QRgb c10 = srcBits[y0 * w + x1];
    const QRgb c01 = srcBits[y1 * w + x0];
    const QRgb c11 = srcBits[y1 * w + x1];

    const float w00 = (1.0f - tx) * (1.0f - ty);
    const float w10 = tx * (1.0f - ty);
    const float w01 = (1.0f - tx) * ty;
    const float w11 = tx * ty;

    const int r = int(qRed(c00) * w00 + qRed(c10) * w10 + qRed(c01) * w01 + qRed(c11) * w11);
    const int g = int(qGreen(c00) * w00 + qGreen(c10) * w10 + qGreen(c01) * w01 + qGreen(c11) * w11);
    const int b = int(qBlue(c00) * w00 + qBlue(c10) * w10 + qBlue(c01) * w01 + qBlue(c11) * w11);
    return qRgb(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}
} // namespace

MilkdropWindow::MilkdropWindow(QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_frontBuffer(kBufW, kBufH, QImage::Format_RGB32)
    , m_backBuffer(kBufW, kBufH, QImage::Format_RGB32)
{
    setWindowTitle(QStringLiteral("Linkamp MilkDrop"));
    setMinimumSize(kMinWindowWidth, kMinWindowHeight);

    const QSettings settings;
    const QSize savedSize(
        settings.value(QStringLiteral("milkdrop/width"), kMinWindowWidth).toInt(),
        settings.value(QStringLiteral("milkdrop/height"), 203).toInt());
    resize(qMax(kMinWindowWidth, savedSize.width()),
           qMax(kMinWindowHeight, savedSize.height()));

    const QString savedPositionKey = QStringLiteral("windows/milkdrop/position");
    if (settings.contains(savedPositionKey)) {
        move(settings.value(savedPositionKey).toPoint());
        setProperty("savedWindowPosition", true);
    }

    setMouseTracking(true);
    m_frontBuffer.fill(Qt::black);
    m_backBuffer.fill(Qt::black);

    m_clock.start();
    m_presetSwitchMs = m_clock.elapsed();
    triggerPresetOverlay();

    m_frameTimer.setInterval(16); // ~60 FPS
    connect(&m_frameTimer, &QTimer::timeout, this, [this] {
        stepSimulation();
        update();
    });
}

void MilkdropWindow::refreshSkin()
{
    update();
}

QRect MilkdropWindow::canvasRect() const
{
    if (m_fullScreen)
        return rect();
    return QRect(kLeftBorderWidth,
                 kTitleBarHeight,
                 std::max(1, width() - kLeftBorderWidth - kRightBorderWidth),
                 std::max(1, height() - kTitleBarHeight - kBottomBarHeight));
}

QRect MilkdropWindow::closeButtonRect() const
{
    if (m_fullScreen)
        return QRect();
    return QRect(width() - 11, 3, 9, 9);
}

void MilkdropWindow::setPlaybackState(MediaPlayer::PlaybackState state)
{
    m_playbackState = state;
}

void MilkdropWindow::setAudioData(const QByteArray &data, QAudioFormat format)
{
    if (format.sampleFormat() != QAudioFormat::Int16)
        return;
    const int bytesPerFrame = format.bytesPerFrame();
    if (bytesPerFrame != 4 || data.size() < kSamples * bytesPerFrame)
        return;

    const qint16 *samples = reinterpret_cast<const qint16 *>(data.constData());
    float mono[N] = {};
    for (int i = 0; i < kSamples; ++i) {
        const float left = float(samples[i * 2]) / 32768.0f;
        const float right = float(samples[i * 2 + 1]) / 32768.0f;
        m_pcmLeft[i] = left;
        m_pcmRight[i] = right;
        if (i < N)
            mono[i] = 0.5f * (left + right);
    }

    float freq[N / 2] = {};
    calc_freq(mono, freq);

    float bassSum = 0.0f;
    float midSum = 0.0f;
    float trebSum = 0.0f;
    for (int i = 0; i < 64; ++i) {
        const int bin0 = (i * (N / 2)) / 64;
        const int bin1 = std::max(bin0 + 1, ((i + 1) * (N / 2)) / 64);
        float accum = 0.0f;
        for (int b = bin0; b < bin1; ++b)
            accum += freq[b];
        const float val = std::clamp(std::sqrt(accum / float(bin1 - bin0)) * 2.8f, 0.0f, 3.0f);
        m_spectrum[i] = m_spectrum[i] * 0.55f + val * 0.45f;
        if (i < 10)
            bassSum += m_spectrum[i];
        else if (i < 32)
            midSum += m_spectrum[i];
        else
            trebSum += m_spectrum[i];
    }

    m_bass = bassSum / 10.0f;
    m_mid = midSum / 22.0f;
    m_treb = trebSum / 32.0f;
    m_lastAudioMs = m_clock.elapsed();
}

void MilkdropWindow::nextPreset()
{
    selectPreset((m_currentPreset + 1) % kPresetCount);
}

void MilkdropWindow::previousPreset()
{
    selectPreset((m_currentPreset + kPresetCount - 1) % kPresetCount);
}

void MilkdropWindow::randomPreset()
{
    if (kPresetCount <= 1)
        return;
    int next = QRandomGenerator::global()->bounded(kPresetCount - 1);
    if (next >= m_currentPreset)
        ++next;
    selectPreset(next);
}

void MilkdropWindow::selectPreset(int index)
{
    const int clamped = ((index % kPresetCount) + kPresetCount) % kPresetCount;
    if (clamped != m_currentPreset) {
        m_previousPreset = m_currentPreset;
        m_currentPreset = clamped;
        m_blendProgress = 0.0f;
    }
    m_presetSwitchMs = m_clock.elapsed();
    triggerPresetOverlay();
}

void MilkdropWindow::triggerPresetOverlay()
{
    m_overlayUntilMs = m_clock.elapsed() + 3500;
}

void MilkdropWindow::toggleFullScreenMode()
{
    m_dragging = false;
    m_resizing = false;
    m_closePressed = false;
    m_lastFullScreenToggleMs = m_clock.elapsed();

    if (m_fullScreen) {
        m_fullScreen = false;
        setWindowState(windowState() & ~Qt::WindowFullScreen);
        showNormal();
        if (!m_windowedGeometry.isEmpty())
            setGeometry(m_windowedGeometry);
        show();
        raise();
        activateWindow();
    } else {
        m_windowedGeometry = geometry();
        m_fullScreen = true;
        QScreen *targetScreen = screen() ? screen() : QGuiApplication::primaryScreen();
        const QRect screenGeo = targetScreen ? targetScreen->geometry() : QRect(0, 0, 1920, 1080);
        setGeometry(screenGeo);
        setWindowState(windowState() | Qt::WindowFullScreen);
        show();
        raise();
        activateWindow();
    }
    update();
}

MilkdropWindow::WarpParams MilkdropWindow::evaluatePresetParams(int presetIndex, float timeSec) const
{
    WarpParams p;
    const float b = m_bass / std::max(0.08f, m_bassAtt);
    const float m = m_mid / std::max(0.08f, m_midAtt);
    const float t = m_treb / std::max(0.08f, m_trebAtt);

    switch (presetIndex % kPresetCount) {
    case 0: // Geiss - Cosmic Dust II
        p.zoom = 1.022f + 0.035f * std::sin(timeSec * 0.7f) + 0.04f * (b - 1.0f);
        p.rot = 0.025f * std::sin(timeSec * 0.53f) + 0.02f * (m - 1.0f);
        p.warp = 0.16f + 0.08f * b;
        p.cx = 0.5f + 0.08f * std::cos(timeSec * 0.41f);
        p.cy = 0.5f + 0.08f * std::sin(timeSec * 0.37f);
        p.decayR = 0.968f;
        p.decayG = 0.955f;
        p.decayB = 0.975f;
        p.colorShift = 0.04f;
        p.waveMode = 0; // Dual circular wave
        break;
    case 1: // Rovastar - Parallel Universe
        p.zoom = 1.045f + 0.05f * m_beatPulse;
        p.rot = 0.04f * std::cos(timeSec * 0.8f);
        p.warp = 0.22f;
        p.decayR = 0.96f;
        p.decayG = 0.972f;
        p.decayB = 0.95f;
        p.colorShift = -0.05f;
        p.waveMode = 1; // Lissajous phase ribbon
        p.symmetry = 4;
        break;
    case 2: // Unchained - Beat Demo (Remix)
        p.zoom = 0.965f - 0.03f * m_beatPulse;
        p.rot = 0.06f * (b - 0.9f) * std::sin(timeSec * 0.45f);
        p.warp = 0.28f + 0.10f * m;
        p.decayR = 0.975f;
        p.decayG = 0.952f;
        p.decayB = 0.962f;
        p.colorShift = 0.06f;
        p.waveMode = 2; // Stereo horizontal wave + shockwave
        break;
    case 3: // Martin - Plasma Tunnel
        p.zoom = 1.075f + 0.04f * b;
        p.rot = 0.032f * std::sin(timeSec * 0.9f);
        p.warp = 0.14f;
        p.dx = 0.008f * std::cos(timeSec * 1.1f);
        p.dy = 0.008f * std::sin(timeSec * 0.95f);
        p.decayR = 0.958f;
        p.decayG = 0.968f;
        p.decayB = 0.978f;
        p.colorShift = 0.07f;
        p.waveMode = 3; // Radial spectrum rays
        break;
    case 4: // Krash & Rovastar - Kaleidoscope Dreams
        p.zoom = 1.03f + 0.03f * std::sin(timeSec * 1.2f);
        p.rot = 0.03f * t;
        p.warp = 0.18f;
        p.decayR = 0.972f;
        p.decayG = 0.962f;
        p.decayB = 0.968f;
        p.colorShift = 0.045f;
        p.waveMode = 4; // Harmonic flower petals
        p.symmetry = 6;
        break;
    case 5: // Zylot - Starfield Warp Drive
        p.zoom = 1.095f + 0.06f * b;
        p.rot = 0.012f * std::sin(timeSec * 0.35f);
        p.warp = 0.06f;
        p.decayR = 0.952f;
        p.decayG = 0.962f;
        p.decayB = 0.980f;
        p.colorShift = 0.025f;
        p.waveMode = 5; // Horizon frequency wave
        break;
    case 6: // Flexi - Predator Prey Spirals
        p.zoom = 1.018f;
        p.rot = 0.05f * std::sin(timeSec * 0.6f);
        p.warp = 0.34f + 0.12f * m;
        p.decayR = 0.970f;
        p.decayG = 0.974f;
        p.decayB = 0.954f;
        p.colorShift = -0.06f;
        p.waveMode = 6; // Spiral ribbon
        break;
    default: // Geiss & Suksma - Feedback Matrix
        p.zoom = 1.038f + 0.03f * (t - 1.0f);
        p.rot = -0.028f * std::cos(timeSec * 0.75f);
        p.warp = 0.24f;
        p.decayR = 0.965f;
        p.decayG = 0.965f;
        p.decayB = 0.975f;
        p.colorShift = 0.055f;
        p.waveMode = 2;
        break;
    }
    return p;
}

void MilkdropWindow::evaluateVertexWarp(int presetIndex, float x, float y,
                                        float rad, float ang, float timeSec,
                                        const WarpParams &p, float *outU, float *outV) const
{
    const float localRot = p.rot + ((presetIndex == 0 || presetIndex == 6)
        ? 0.09f * (0.6f - rad) * std::sin(timeSec * 0.9f)
        : (presetIndex == 3 ? 0.12f / (rad + 0.25f) * std::sin(timeSec * 0.6f) : 0.0f));

    const float localZoom = p.zoom + ((presetIndex == 2)
        ? 0.08f * std::sin(rad * 6.0f - timeSec * 3.2f)
        : (presetIndex == 3 ? 0.09f * rad : 0.0f));

    const float cosR = std::cos(localRot);
    const float sinR = std::sin(localRot);

    float wx = x / localZoom;
    float wy = y / localZoom;

    const float rx = wx * cosR - wy * sinR;
    const float ry = wx * sinR + wy * cosR;

    const float warpX = p.warp * 0.06f * (
        std::sin(ry * 5.2f + timeSec * 1.9f) +
        std::cos(rx * 4.1f - timeSec * 1.4f));
    const float warpY = p.warp * 0.06f * (
        std::cos(rx * 5.2f + timeSec * 1.7f) -
        std::sin(ry * 4.3f + timeSec * 2.1f));

    float u = 0.5f + (rx + warpX + p.dx) * 0.5f + (p.cx - 0.5f) * 0.08f;
    float v = 0.5f + (ry + warpY + p.dy) * 0.5f + (p.cy - 0.5f) * 0.08f;

    if (p.symmetry == 4) {
        u = 0.5f - std::abs(u - 0.5f);
        v = 0.5f - std::abs(v - 0.5f);
        u = 0.25f + u * 0.5f;
        v = 0.25f + v * 0.5f;
    } else if (p.symmetry == 6) {
        const float foldAng = std::abs(std::fmod(ang + 3.14159265f, 3.14159265f / 3.0f) - 3.14159265f / 6.0f);
        const float foldRad = rad / localZoom;
        u = 0.5f + 0.5f * foldRad * std::cos(foldAng + timeSec * 0.25f);
        v = 0.5f + 0.5f * foldRad * std::sin(foldAng + timeSec * 0.25f);
    }

    // Mirror wrap around [0, 1] so edges never clamp to flat streaks
    auto mirrorWrap = [](float val) {
        val = std::fmod(val, 2.0f);
        if (val < 0.0f)
            val += 2.0f;
        return val > 1.0f ? 2.0f - val : val;
    };

    *outU = mirrorWrap(u);
    *outV = mirrorWrap(v);
}

void MilkdropWindow::stepSimulation()
{
    const qint64 nowMs = m_clock.elapsed();
    const float timeSec = float(nowMs) * 0.001f;

    // If audio hasn't arrived in >120ms, synthesize a gentle idle signal so
    // MilkDrop still swirls hypnotically when paused or stopped.
    const bool audioActive = (m_playbackState == MediaPlayer::PlayingState)
                             && (nowMs - m_lastAudioMs < 200);
    if (!audioActive) {
        for (int i = 0; i < kSamples; ++i) {
            const float ph = float(i) / float(kSamples) * 6.2831853f;
            m_pcmLeft[i] = 0.28f * std::sin(ph * 2.0f + timeSec * 2.4f)
                         + 0.14f * std::cos(ph * 5.0f - timeSec * 1.7f);
            m_pcmRight[i] = 0.28f * std::cos(ph * 2.0f + timeSec * 2.1f)
                          + 0.14f * std::sin(ph * 3.0f + timeSec * 3.1f);
        }
        for (int i = 0; i < 64; ++i) {
            const float bandPhase = float(i) * 0.18f;
            m_spectrum[i] = 0.22f + 0.18f * std::sin(timeSec * 2.2f + bandPhase)
                                  + 0.10f * std::cos(timeSec * 3.7f - bandPhase * 1.5f);
        }
        m_bass = 0.35f + 0.20f * std::sin(timeSec * 2.1f);
        m_mid = 0.30f + 0.15f * std::cos(timeSec * 1.7f);
        m_treb = 0.25f + 0.15f * std::sin(timeSec * 2.9f);
    }

    m_bassAtt = m_bassAtt * 0.93f + m_bass * 0.07f;
    m_midAtt = m_midAtt * 0.93f + m_mid * 0.07f;
    m_trebAtt = m_trebAtt * 0.93f + m_treb * 0.07f;

    // Beat detection
    if (m_bass > m_bassAtt * 1.32f && m_bass > 0.22f && (nowMs - m_lastBeatMs) > 220) {
        m_beatPulse = 1.0f;
        m_lastBeatMs = nowMs;
    } else {
        m_beatPulse *= 0.86f;
    }

    // Auto-cycle presets every 16 seconds
    if (m_autoCycle && (nowMs - m_presetSwitchMs) > 16000) {
        nextPreset();
    }

    if (m_blendProgress < 1.0f) {
        m_blendProgress = std::min(1.0f, m_blendProgress + 0.035f);
    }

    const WarpParams curP = evaluatePresetParams(m_currentPreset, timeSec);
    const WarpParams prevP = (m_blendProgress < 1.0f)
        ? evaluatePresetParams(m_previousPreset, timeSec)
        : curP;

    const float blend = m_blendProgress * m_blendProgress * (3.0f - 2.0f * m_blendProgress);

    // Evaluate warp grid vertices
    float gridU[kGridY + 1][kGridX + 1];
    float gridV[kGridY + 1][kGridX + 1];

    for (int gy = 0; gy <= kGridY; ++gy) {
        const float ny = (float(gy) / float(kGridY)) * 2.0f - 1.0f;
        for (int gx = 0; gx <= kGridX; ++gx) {
            const float nx = (float(gx) / float(kGridX)) * 2.0f - 1.0f;
            const float rad = std::hypot(nx, ny);
            const float ang = std::atan2(ny, nx);

            float u1 = 0.5f, v1 = 0.5f;
            evaluateVertexWarp(m_currentPreset, nx, ny, rad, ang, timeSec, curP, &u1, &v1);
            if (m_blendProgress < 1.0f) {
                float u0 = 0.5f, v0 = 0.5f;
                evaluateVertexWarp(m_previousPreset, nx, ny, rad, ang, timeSec, prevP, &u0, &v0);
                gridU[gy][gx] = u0 * (1.0f - blend) + u1 * blend;
                gridV[gy][gx] = v0 * (1.0f - blend) + v1 * blend;
            } else {
                gridU[gy][gx] = u1;
                gridV[gy][gx] = v1;
            }
        }
    }

    const float decayR = prevP.decayR * (1.0f - blend) + curP.decayR * blend;
    const float decayG = prevP.decayG * (1.0f - blend) + curP.decayG * blend;
    const float decayB = prevP.decayB * (1.0f - blend) + curP.decayB * blend;
    const float shift = prevP.colorShift * (1.0f - blend) + curP.colorShift * blend;

    const QRgb *srcBits = reinterpret_cast<const QRgb *>(m_frontBuffer.constBits());
    QRgb *dstBits = reinterpret_cast<QRgb *>(m_backBuffer.bits());

    for (int py = 0; py < kBufH; ++py) {
        const float gyFloat = (float(py) / float(kBufH - 1)) * float(kGridY);
        const int gy0 = std::min(int(gyFloat), kGridY - 1);
        const float ty = gyFloat - float(gy0);

        for (int px = 0; px < kBufW; ++px) {
            const float gxFloat = (float(px) / float(kBufW - 1)) * float(kGridX);
            const int gx0 = std::min(int(gxFloat), kGridX - 1);
            const float tx = gxFloat - float(gx0);

            const float u00 = gridU[gy0][gx0];
            const float u10 = gridU[gy0][gx0 + 1];
            const float u01 = gridU[gy0 + 1][gx0];
            const float u11 = gridU[gy0 + 1][gx0 + 1];

            const float v00 = gridV[gy0][gx0];
            const float v10 = gridV[gy0][gx0 + 1];
            const float v01 = gridV[gy0 + 1][gx0];
            const float v11 = gridV[gy0 + 1][gx0 + 1];

            const float u = (u00 * (1.0f - tx) + u10 * tx) * (1.0f - ty)
                          + (u01 * (1.0f - tx) + u11 * tx) * ty;
            const float v = (v00 * (1.0f - tx) + v10 * tx) * (1.0f - ty)
                          + (v01 * (1.0f - tx) + v11 * tx) * ty;

            const QRgb sampled = sampleBilinear(srcBits, kBufW, kBufH, u, v);
            const float sr = float(qRed(sampled));
            const float sg = float(qGreen(sampled));
            const float sb = float(qBlue(sampled));

            // Subtle MilkDrop RGB channel cross-feed & decay
            const int nr = std::clamp(int((sr + shift * (sg - sb)) * decayR), 0, 255);
            const int ng = std::clamp(int((sg + shift * (sb - sr)) * decayG), 0, 255);
            const int nb = std::clamp(int((sb + shift * (sr - sg)) * decayB), 0, 255);

            dstBits[py * kBufW + px] = qRgb(nr, ng, nb);
        }
    }

    QPainter bufPainter(&m_backBuffer);
    bufPainter.setRenderHint(QPainter::Antialiasing, true);
    renderWaveforms(bufPainter, timeSec);
    bufPainter.end();

    m_frontBuffer.swap(m_backBuffer);
}

void MilkdropWindow::renderWaveforms(QPainter &bufPainter, float timeSec)
{
    const float cx = kBufW * 0.5f;
    const float cy = kBufH * 0.5f;
    const float minDim = std::min(kBufW, kBufH) * 0.42f;

    const int hue1 = int(std::fmod(timeSec * 52.0f + m_bass * 90.0f, 360.0f));
    const int hue2 = (hue1 + 115) % 360;
    const int hue3 = (hue1 + 235) % 360;

    const QColor primaryColor = QColor::fromHsv(hue1, 230, 255, 225);
    const QColor secondaryColor = QColor::fromHsv(hue2, 220, 255, 190);
    const QColor accentColor = QColor::fromHsv(hue3, 200, 255, 165);

    // Beat-reactive custom shape / outer pulse
    if (m_beatPulse > 0.12f) {
        QPen pulsePen(QColor::fromHsv((hue1 + 60) % 360, 190, 255, int(m_beatPulse * 140.0f)));
        pulsePen.setWidthF(1.4f + m_beatPulse * 1.5f);
        bufPainter.setPen(pulsePen);
        bufPainter.setBrush(Qt::NoBrush);
        const float r = minDim * (0.35f + (1.0f - m_beatPulse) * 0.75f);
        bufPainter.drawEllipse(QPointF(cx, cy), r, r);
    }

    const int waveMode = evaluatePresetParams(m_currentPreset, timeSec).waveMode;

    if (waveMode == 0) {
        // Dual circular MilkDrop waveform (Geiss - Cosmic Dust)
        QPainterPath path1;
        QPainterPath path2;
        constexpr int points = 128;
        for (int i = 0; i <= points; ++i) {
            const int idx = (i % points) * (kSamples / points);
            const float angle = (float(i) / float(points)) * 6.2831853f + timeSec * 0.6f;
            const float r1 = minDim * (0.58f + 0.42f * m_pcmLeft[idx] + 0.12f * m_bass);
            const float r2 = minDim * (0.36f + 0.35f * m_pcmRight[idx] + 0.10f * m_treb);
            const QPointF pt1(cx + std::cos(angle) * r1, cy + std::sin(angle) * r1);
            const QPointF pt2(cx + std::cos(-angle * 1.5f) * r2, cy + std::sin(-angle * 1.5f) * r2);
            if (i == 0) {
                path1.moveTo(pt1);
                path2.moveTo(pt2);
            } else {
                path1.lineTo(pt1);
                path2.lineTo(pt2);
            }
        }
        bufPainter.setPen(QPen(primaryColor, 1.8));
        bufPainter.drawPath(path1);
        bufPainter.setPen(QPen(secondaryColor, 1.3));
        bufPainter.drawPath(path2);
    } else if (waveMode == 1) {
        // Lissajous stereo phase curve (Rovastar - Parallel Universe)
        QPainterPath path;
        constexpr int points = 160;
        for (int i = 0; i < points; ++i) {
            const int idx = i * (kSamples / points);
            const float t = float(i) / float(points - 1);
            const float x = cx + (m_pcmLeft[idx] * 0.75f + 0.35f * std::sin(t * 6.283f + timeSec * 1.7f)) * (kBufW * 0.42f);
            const float y = cy + (m_pcmRight[idx] * 0.75f + 0.35f * std::cos(t * 9.424f - timeSec * 1.3f)) * (kBufH * 0.42f);
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        bufPainter.setPen(QPen(primaryColor, 2.0));
        bufPainter.drawPath(path);
    } else if (waveMode == 2) {
        // Dual stereo ribbon waves (Unchained / Feedback Matrix)
        QPainterPath topWave;
        QPainterPath botWave;
        constexpr int points = 128;
        for (int i = 0; i < points; ++i) {
            const int idx = i * (kSamples / points);
            const float x = (float(i) / float(points - 1)) * float(kBufW);
            const float y1 = cy - 16.0f + m_pcmLeft[idx] * (kBufH * 0.36f);
            const float y2 = cy + 16.0f + m_pcmRight[idx] * (kBufH * 0.36f);
            if (i == 0) {
                topWave.moveTo(x, y1);
                botWave.moveTo(x, y2);
            } else {
                topWave.lineTo(x, y1);
                botWave.lineTo(x, y2);
            }
        }
        bufPainter.setPen(QPen(primaryColor, 1.9));
        bufPainter.drawPath(topWave);
        bufPainter.setPen(QPen(secondaryColor, 1.6));
        bufPainter.drawPath(botWave);
    } else if (waveMode == 3) {
        // Radial plasma spectrum starburst (Martin - Plasma Tunnel)
        for (int i = 0; i < 48; ++i) {
            const float angle = (float(i) / 48.0f) * 6.2831853f + timeSec * 0.8f;
            const float mag = std::clamp(m_spectrum[i], 0.05f, 1.8f);
            const float r0 = minDim * 0.16f;
            const float r1 = minDim * (0.24f + mag * 0.55f);
            const QColor rayColor = QColor::fromHsv((hue1 + i * 6) % 360, 235, 255, 210);
            bufPainter.setPen(QPen(rayColor, 1.6));
            bufPainter.drawLine(QPointF(cx + std::cos(angle) * r0, cy + std::sin(angle) * r0),
                                QPointF(cx + std::cos(angle) * r1, cy + std::sin(angle) * r1));
        }
    } else if (waveMode == 4) {
        // Harmonic rose petals (Kaleidoscope Dreams)
        QPainterPath path;
        constexpr int points = 160;
        for (int i = 0; i <= points; ++i) {
            const int idx = (i % points) * (kSamples / points);
            const float theta = (float(i) / float(points)) * 6.2831853f;
            const float petal = 0.55f + 0.35f * std::cos(6.0f * theta + timeSec * 1.5f)
                              + 0.32f * m_pcmLeft[idx];
            const float r = minDim * petal;
            const QPointF pt(cx + std::cos(theta) * r, cy + std::sin(theta) * r);
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        bufPainter.setPen(QPen(primaryColor, 1.8));
        bufPainter.drawPath(path);
        bufPainter.setPen(QPen(accentColor, 1.2));
        bufPainter.drawEllipse(QPointF(cx, cy), minDim * (0.20f + 0.15f * m_treb),
                               minDim * (0.20f + 0.15f * m_treb));
    } else if (waveMode == 5) {
        // Starfield warp seeds + horizon wave (Zylot)
        QPainterPath path;
        for (int i = 0; i < 64; ++i) {
            const float x = (float(i) / 63.0f) * float(kBufW);
            const float y = cy + (0.25f - m_spectrum[i] * 0.45f) * float(kBufH * 0.55f)
                          * std::sin(float(i) * 0.3f + timeSec * 3.0f);
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        bufPainter.setPen(QPen(primaryColor, 1.8));
        bufPainter.drawPath(path);
        for (int s = 0; s < 14; ++s) {
            const float a = float(s) * 0.44879f + timeSec * 1.9f;
            const float rad = 6.0f + (s % 5) * 4.0f;
            bufPainter.setPen(QPen(secondaryColor, 2.0));
            bufPainter.drawPoint(QPointF(cx + std::cos(a) * rad, cy + std::sin(a) * rad));
        }
    } else {
        // Twisting double helix (Flexi - Predator Prey Spirals)
        QPainterPath helix1;
        QPainterPath helix2;
        constexpr int points = 120;
        for (int i = 0; i < points; ++i) {
            const int idx = i * (kSamples / points);
            const float t = float(i) / float(points - 1);
            const float ang = t * 12.566f - timeSec * 2.8f;
            const float rad = t * minDim * (0.85f + 0.35f * m_pcmLeft[idx]);
            const QPointF p1(cx + std::cos(ang) * rad, cy + std::sin(ang) * rad);
            const QPointF p2(cx + std::cos(ang + 3.14159f) * rad, cy + std::sin(ang + 3.14159f) * rad);
            if (i == 0) {
                helix1.moveTo(p1);
                helix2.moveTo(p2);
            } else {
                helix1.lineTo(p1);
                helix2.lineTo(p2);
            }
        }
        bufPainter.setPen(QPen(primaryColor, 1.8));
        bufPainter.drawPath(helix1);
        bufPainter.setPen(QPen(secondaryColor, 1.5));
        bufPainter.drawPath(helix2);
    }
}

void MilkdropWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    const QRect targetCanvas = canvasRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(targetCanvas, m_frontBuffer);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // Draw MilkDrop preset title OSD overlay if recently switched
    const qint64 nowMs = m_clock.elapsed();
    if (nowMs < m_overlayUntilMs) {
        const qint64 remaining = m_overlayUntilMs - nowMs;
        const int alpha = std::clamp(int(remaining * 255 / 700), 0, 230);
        QFont osdFont(QStringLiteral("Arial"));
        osdFont.setPixelSize(m_fullScreen ? 18 : 11);
        osdFont.setBold(true);
        painter.setFont(osdFont);
        const QString label = QString::fromLatin1(kPresetNames[m_currentPreset]);
        const QRect textRect = targetCanvas.adjusted(6, 4, -6, -4);
        painter.setPen(QColor(0, 0, 0, alpha));
        painter.drawText(textRect.translated(1, 1), Qt::AlignLeft | Qt::AlignTop, label);
        painter.setPen(QColor(255, 240, 150, alpha));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, label);
    }

    if (m_fullScreen)
        return;

    // Draw authentic Winamp GEN.BMP window frame
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

    // Titlebar layout:
    // [0: left corner 25px] [left gold tiles] [26: left cap 25px] [52: 3x25=75px dark capsule] [78: right cap 25px] [right gold tiles] [130: right corner 25px]
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

    drawGenTitleText(painter, genPixmap, genImage, QStringLiteral("MILKDROP"),
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

void MilkdropWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_presetSwitchMs = m_clock.elapsed();
    triggerPresetOverlay();
    if (!m_frameTimer.isActive())
        m_frameTimer.start();
}

void MilkdropWindow::hideEvent(QHideEvent *event)
{
    m_frameTimer.stop();
    QDialog::hideEvent(event);
}

void MilkdropWindow::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (!m_fullScreen) {
        QSettings settings;
        settings.setValue(QStringLiteral("milkdrop/width"), width());
        settings.setValue(QStringLiteral("milkdrop/height"), height());
    }
}

void MilkdropWindow::moveEvent(QMoveEvent *event)
{
    QDialog::moveEvent(event);
    if (isVisible() && !m_fullScreen) {
        QSettings().setValue(QStringLiteral("windows/milkdrop/position"),
                             frameGeometry().topLeft());
    }
}

void MilkdropWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange)
        update();
    QDialog::changeEvent(event);
}

void MilkdropWindow::mousePressEvent(QMouseEvent *event)
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

void MilkdropWindow::mouseMoveEvent(QMouseEvent *event)
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

void MilkdropWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_closePressed) {
        const bool shouldClose = closeButtonRect().contains(event->position().toPoint());
        m_closePressed = false;
        update();
        if (shouldClose) {
            QSettings().setValue(QStringLiteral("windows/milkdrop/visible"), false);
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

void MilkdropWindow::mouseDoubleClickEvent(QMouseEvent *event)
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

void MilkdropWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_N:
    case Qt::Key_Right:
        nextPreset();
        event->accept();
        return;
    case Qt::Key_P:
    case Qt::Key_Backspace:
    case Qt::Key_Left:
        previousPreset();
        event->accept();
        return;
    case Qt::Key_R:
        randomPreset();
        event->accept();
        return;
    case Qt::Key_L:
        triggerPresetOverlay();
        event->accept();
        return;
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

void MilkdropWindow::showContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QMenu *presetsMenu = menu.addMenu(tr("Preset MilkDrop"));
    for (int i = 0; i < kPresetCount; ++i) {
        QAction *act = presetsMenu->addAction(QString::fromLatin1(kPresetNames[i]));
        act->setCheckable(true);
        act->setChecked(i == m_currentPreset);
        connect(act, &QAction::triggered, this, [this, i] { selectPreset(i); });
    }
    menu.addSeparator();
    QAction *nextAct = menu.addAction(tr("Preset successivo (Spazio / N)"));
    QAction *prevAct = menu.addAction(tr("Preset precedente (P)"));
    QAction *randAct = menu.addAction(tr("Preset casuale (R)"));
    menu.addSeparator();
    QAction *autoAct = menu.addAction(tr("Cambio preset automatico"));
    autoAct->setCheckable(true);
    autoAct->setChecked(m_autoCycle);
    QAction *fullAct = menu.addAction(tr("Schermo intero (Doppio clic / F)"));
    fullAct->setCheckable(true);
    fullAct->setChecked(m_fullScreen);
    menu.addSeparator();
    QAction *closeAct = menu.addAction(tr("Chiudi MilkDrop"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == nextAct)
        nextPreset();
    else if (chosen == prevAct)
        previousPreset();
    else if (chosen == randAct)
        randomPreset();
    else if (chosen == autoAct)
        m_autoCycle = autoAct->isChecked();
    else if (chosen == fullAct)
        toggleFullScreenMode();
    else if (chosen == closeAct) {
        QSettings().setValue(QStringLiteral("windows/milkdrop/visible"), false);
        hide();
    }
}
