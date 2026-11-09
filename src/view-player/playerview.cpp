#include "playerview.h"
#include "ui_playerview.h"

#include "scale.h"
#include "util.h"
#include "winampskin.h"

#include <QApplication>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QDir>
#include <QFileDialog>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaMetaData>
#include <QMessageBox>
#include <QStandardPaths>
#include <QFontDatabase>
#include <QSettings>
#include <QSpacerItem>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

PlayerView::PlayerView(QWidget *parent, ControlButtonsWidget *ctlBtns) :
    QWidget(parent),
    ui(new Ui::PlayerView)
{
    // Load custom fonts
    QFontDatabase::addApplicationFont(":/assets/bignumbers.ttf");

    // Setup UI
    ui->setupUi(this);
    scale();
    m_pauseDash = new QWidget(ui->visualizationBackground);
    m_pauseDash->setObjectName(QStringLiteral("pauseTimeDash"));
    m_pauseDash->setGeometry(26 * UI_SCALE, 9 * UI_SCALE,
                             5 * UI_SCALE, UI_SCALE);
    m_pauseDash->setStyleSheet(QStringLiteral(
        "background-color: #00f800; border: none;"));
    m_pauseDash->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_pauseDash->hide();
    // These indicators overlap the spectrum grid by design, like Winamp's
    // masked PLAYPAUS.BMP blit. Keep them above the spectrum child widget.
    ui->playStatusIcon->raise();
    m_pauseDash->raise();
    m_timeLeftMode = QSettings().value(QStringLiteral("player/timeLeftMode"), false).toBool();
    ui->progressTimeLabel->installEventFilter(this);
    ui->posBar->installEventFilter(this);
    m_pauseBlinkTimer.setInterval(500);
    connect(&m_pauseBlinkTimer, &QTimer::timeout, this, [this] {
        m_pauseTimeVisible = !m_pauseTimeVisible;
        ui->progressTimeLabel->setVisible(m_pauseTimeVisible);
    });
    // Keep the Classic volume controls out of the compressed right-column
    // layout. Their 13px sprite tracks extend below that row and get clipped
    // as soon as the seek bar becomes active.
    QHBoxLayout *controlsLayout = ui->horizontalLayout_7;
    controlsLayout->removeWidget(ui->volumeSlider);
    controlsLayout->removeWidget(ui->balanceSlider);
    controlsLayout->removeWidget(ui->eqButton);
    controlsLayout->removeWidget(ui->playlistButton);
    ui->volumeSlider->setParent(this);
    ui->balanceSlider->setParent(this);
    ui->eqButton->setParent(this);
    ui->playlistButton->setParent(this);
    controlsLayout->insertSpacerItem(0, new QSpacerItem(
        68 * UI_SCALE, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
    controlsLayout->insertSpacerItem(2, new QSpacerItem(
        38 * UI_SCALE, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
    controlsLayout->insertSpacerItem(4, new QSpacerItem(
        23 * UI_SCALE, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
    controlsLayout->insertSpacerItem(5, new QSpacerItem(
        23 * UI_SCALE, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
    ui->volumeSlider->setMinimumHeight(13 * UI_SCALE);
    ui->volumeSlider->setMaximumHeight(13 * UI_SCALE);
    ui->balanceSlider->setMinimumHeight(13 * UI_SCALE);
    ui->balanceSlider->setMaximumHeight(13 * UI_SCALE);
    ui->volumeSlider->setGeometry(103 * UI_SCALE, 42 * UI_SCALE,
                                  68 * UI_SCALE, 13 * UI_SCALE);
    ui->balanceSlider->setGeometry(173 * UI_SCALE, 42 * UI_SCALE,
                                   38 * UI_SCALE, 13 * UI_SCALE);
    ui->eqButton->setGeometry(215 * UI_SCALE, 43 * UI_SCALE,
                              23 * UI_SCALE, 12 * UI_SCALE);
    ui->playlistButton->setGeometry(238 * UI_SCALE, 43 * UI_SCALE,
                                    23 * UI_SCALE, 12 * UI_SCALE);
    ui->volumeSlider->raise();
    ui->balanceSlider->raise();
    ui->eqButton->raise();
    ui->playlistButton->raise();
    auto *visualModeLabels = new QLabel(this);
    visualModeLabels->setObjectName(QStringLiteral("classicVisualModeLabels"));
    visualModeLabels->setAttribute(Qt::WA_TransparentForMouseEvents);
    visualModeLabels->setStyleSheet(QStringLiteral("background: transparent;"));
    const QImage titlebarAtlas = WinampSkin::instance().sheet(QStringLiteral("titlebar-atlas"));
    if (!titlebarAtlas.isNull())
        visualModeLabels->setPixmap(QPixmap::fromImage(titlebarAtlas.copy(304, 0, 8, 43)));
    if (window())
        window()->installEventFilter(this);
    // Winamp draws this titlebar.bmp strip at main-window coordinates
    // (10, 22), size 8x43. PlayerView is at (4, 14) in MainWindow.
    visualModeLabels->setGeometry(QRect(
        QPoint(6 * UI_SCALE, 8 * UI_SCALE),
        QSize(8 * UI_SCALE, 43 * UI_SCALE)));
    ui->monoLabel->hide();
    ui->stereoLabel->hide();
    ui->kbpsFrame->hide();
    ui->khzFrame->hide();
    m_monoIndicator = new QLabel(this);
    m_monoIndicator->setObjectName(QStringLiteral("classicMonoIndicator"));
    m_monoIndicator->setGeometry(208 * UI_SCALE, 26 * UI_SCALE, 27 * UI_SCALE, 12 * UI_SCALE);
    m_monoIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_stereoIndicator = new QLabel(this);
    m_stereoIndicator->setObjectName(QStringLiteral("classicStereoIndicator"));
    m_stereoIndicator->setGeometry(235 * UI_SCALE, 26 * UI_SCALE, 29 * UI_SCALE, 12 * UI_SCALE);
    m_stereoIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    setChannelIndicators(false, false);

    ui->kbpsValueLabel->setParent(this);
    ui->khzValueLabel->setParent(this);
    ui->kbpsValueLabel->setGeometry(107 * UI_SCALE, 28 * UI_SCALE, 15 * UI_SCALE, 6 * UI_SCALE);
    ui->khzValueLabel->setGeometry(152 * UI_SCALE, 28 * UI_SCALE, 10 * UI_SCALE, 6 * UI_SCALE);
    ui->kbpsValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->khzValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->kbpsValueLabel->setStyleSheet(QStringLiteral("background: transparent; padding: 0px; margin: 0px; border: none;"));
    ui->khzValueLabel->setStyleSheet(QStringLiteral("background: transparent; padding: 0px; margin: 0px; border: none;"));
    ui->kbpsValueLabel->raise();
    ui->khzValueLabel->raise();

    ui->posBarContainer->layout()->removeWidget(ui->posBar);
    ui->posBar->setParent(this);
    ui->posBar->setMinimumHeight(10 * UI_SCALE);
    ui->posBar->setMaximumHeight(10 * UI_SCALE);
    ui->posBar->setGeometry(12 * UI_SCALE, 57 * UI_SCALE,
                            248 * UI_SCALE, 10 * UI_SCALE);
    ui->posBar->raise();
    QFont tickerFont = ui->songInfoLabel->font();
    tickerFont.setFamily(QStringLiteral("DejaVu Sans Mono"));
    tickerFont.setPixelSize(6 * UI_SCALE);
    tickerFont.setBold(true);
    tickerFont.setStyleStrategy(QFont::NoAntialias);
    ui->songInfoLabel->setFont(tickerFont);
    const auto setPixelLabelFont = [](QLabel *label) {
        QFont font = label->font();
        font.setFamily(QStringLiteral("DejaVu Sans Mono"));
        font.setPixelSize(6 * UI_SCALE);
        font.setBold(true);
        font.setStyleStrategy(QFont::NoAntialias);
        label->setFont(font);
    };
    for (QLabel *label : {ui->kbpsLabel, ui->kbpsValueLabel, ui->kHzLabel,
                          ui->khzValueLabel, ui->monoLabel, ui->stereoLabel}) {
        setPixelLabelFont(label);
    }

    controlButtons = ctlBtns;

    connect(controlButtons, &ControlButtonsWidget::playClicked, this, &PlayerView::playClicked);
    connect(controlButtons, &ControlButtonsWidget::pauseClicked, this, &PlayerView::pauseClicked);
    connect(controlButtons, &ControlButtonsWidget::stopClicked, this, &PlayerView::stopClicked);
    connect(controlButtons, &ControlButtonsWidget::nextClicked, this, &PlayerView::nextClicked);
    connect(controlButtons, &ControlButtonsWidget::previousClicked, this, &PlayerView::previousClicked);
    connect(controlButtons, &ControlButtonsWidget::openClicked, this,  &PlayerView::openClicked);
    connect(controlButtons, &ControlButtonsWidget::repeatClicked, this, &PlayerView::repeatClicked);
    connect(controlButtons, &ControlButtonsWidget::shuffleClicked, this, &PlayerView::shuffleClicked);

    // Setup spectrum analyzer
    spectrum = new SpectrumWidget(this);

    // duration slider and label
    ui->posBar->setRange(0, 0);
    ui->posBar->setVisible(true);
    connect(ui->posBar, &QSlider::sliderPressed, this, [this]() {
        if (!m_deferredSeekEnabled) {
            return;
        }
        m_isUserSeeking = true;
    });
    connect(ui->posBar, &QSlider::sliderMoved, this, [this](int value) {
        if (!m_deferredSeekEnabled) {
            emit positionChanged(value);
            return;
        }

        // Preview time while dragging, but do not seek yet.
        updateDurationInfo(value / 1000);
    });
    connect(ui->posBar, &QSlider::sliderReleased, this, [this]() {
        if (!m_deferredSeekEnabled) {
            return;
        }

        m_isUserSeeking = false;
        const int value = ui->posBar->value();
        updateDurationInfo(value / 1000);
        emit positionChanged(value);
    });

    // Set volume slider
    ui->volumeSlider->setRange(0, 100);
    QList<QColor> volumeGradientColors{
        QColor::fromRgb(107, 59, 18),
        QColor::fromRgb(125, 70, 22),
        QColor::fromRgb(138, 79, 25),
        QColor::fromRgb(155, 93, 33),
        QColor::fromRgb(165, 100, 36),
        QColor::fromRgb(175, 108, 38),
        QColor::fromRgb(184, 112, 40),
    };
    ui->volumeSlider->setGradient(volumeGradientColors, Qt::Horizontal);
    connect(ui->volumeSlider, &QSlider::sliderPressed, this, &PlayerView::volumeDragStarted);
    connect(ui->volumeSlider, &QSlider::sliderReleased, this, [this]() {
        emit volumeDragFinished(ui->volumeSlider->value());
    });
    connect(ui->volumeSlider, &QSlider::valueChanged, this, &PlayerView::volumeChanged);

    // Set Balance Slider
    ui->balanceSlider->setRange(-100, 100);
    QList<QColor> balanceGradientColors;
    // balance gradient is two concatenated volumeGradients, but one reversed so green is center
    for(int i = volumeGradientColors.count() - 1; i >= 0; i--) {
        balanceGradientColors.append(volumeGradientColors[i]);
    }
    for(int i = 0; i < volumeGradientColors.count(); i++) {
        balanceGradientColors.append(volumeGradientColors[i]);
    }
    ui->balanceSlider->setGradient(balanceGradientColors, Qt::Horizontal);
    connect(ui->balanceSlider, &QSlider::sliderPressed, this, &PlayerView::balanceDragStarted);
    connect(ui->balanceSlider, &QSlider::sliderReleased, this, [this]() {
        emit balanceDragFinished(ui->balanceSlider->value());
    });
    connect(ui->balanceSlider, &QSlider::valueChanged, this, &PlayerView::handleBalanceChanged);

    // Reset time counter and labels
    ui->progressTimeLabel->setText("");
    const int playerVolume = qBound(0, QSettings().value(QStringLiteral("player/volume"), 84).toInt(), 100);
    ui->volumeSlider->setValue(playerVolume);
    connect(ui->volumeSlider, &QSlider::valueChanged, this, [](int value) {
        QSettings().setValue(QStringLiteral("player/volume"), value);
    });
    ui->balanceSlider->setValue(0);
    ui->playlistButton->setChecked(true);
    ui->eqButton->setChecked(false);
    m_kbpsText.clear();
    m_khzText.clear();
    updateRateLabels();
    ui->inputLabel->hide();
    ui->kHzLabel->hide();
    // MAIN.BMP already contains the white "kbps" legend. Keep the label's
    // layout width so the second value frame stays over its bitmap box, while
    // making the duplicate QLabel text transparent.
    ui->kbpsLabel->setFixedWidth(17 * UI_SCALE);
    ui->kbpsLabel->setStyleSheet(QStringLiteral("color: transparent; background: transparent;"));
    ui->kbpsLabel->show();

    // Set play status icon
    setPlaybackState(MediaPlayer::StoppedState);

    connect(ui->playlistButton, &QCheckBox::clicked, this, &PlayerView::plClicked);
    connect(ui->eqButton, &QCheckBox::clicked, this, [this](bool) {
        ui->eqButton->setChecked(eqEnabled);
        emit eqClicked();
    });

    // Setup spectrum widget
    QVBoxLayout *spectrumLayout = new QVBoxLayout;
    spectrumLayout->addWidget(spectrum);
    spectrumLayout->setContentsMargins(0, 0, 0, 0);
    spectrumLayout->setSpacing(0);
    ui->spectrumContainer->setLayout(spectrumLayout);

    // Setup message functionality
    messageTimer = new QTimer(this);
    connect(messageTimer, &QTimer::timeout, this, &PlayerView::clearMessage);
}

PlayerView::~PlayerView()
{
    delete ui;
}

void PlayerView::scale()
{
    this->setMaximumSize(this->maximumSize() * UI_SCALE);
    this->setMinimumSize(this->minimumSize() * UI_SCALE);

    ui->posBarContainer->layout()->setContentsMargins(ui->posBarContainer->layout()->contentsMargins() * UI_SCALE);

    ui->posBar->setMaximumHeight(10 * UI_SCALE);
    ui->posBar->setMinimumHeight(10 * UI_SCALE);
    updatePositionSliderStyle();

    ui->infoContainer->setContentsMargins(ui->infoContainer->contentsMargins() * UI_SCALE);
    QMargins visualizationMargins = ui->visualizationContainer->contentsMargins() * UI_SCALE;
    visualizationMargins.setTop(std::max(0, visualizationMargins.top() - UI_SCALE));
    ui->visualizationContainer->setContentsMargins(visualizationMargins);

    ui->codecDetailsContainer->layout()->setContentsMargins(ui->codecDetailsContainer->layout()->contentsMargins() * UI_SCALE);
    ui->codecDetailsContainer->layout()->setSpacing(ui->codecDetailsContainer->layout()->spacing() * UI_SCALE);
    ui->horizontalSpacer_5->changeSize(6 * UI_SCALE, UI_SCALE,
                                       QSizePolicy::Fixed, QSizePolicy::Minimum);
    ui->codecDetailsContainer->setStyleSheet(getStylesheet("playerview.codecDetailsContainer"));


    ui->kHzLabel->setMaximumHeight(ui->kHzLabel->maximumHeight() * UI_SCALE);
    ui->kHzLabel->setMinimumHeight(ui->kHzLabel->minimumHeight() * UI_SCALE);

    ui->kbpsLabel->setMaximumHeight(ui->kbpsLabel->maximumHeight() * UI_SCALE);
    ui->kbpsLabel->setMinimumHeight(ui->kbpsLabel->minimumHeight() * UI_SCALE);

    ui->kbpsValueLabel->setGeometry(107 * UI_SCALE, 28 * UI_SCALE, 15 * UI_SCALE, 6 * UI_SCALE);
    ui->khzValueLabel->setGeometry(152 * UI_SCALE, 28 * UI_SCALE, 10 * UI_SCALE, 6 * UI_SCALE);

    ui->monoLabel->setMinimumHeight(ui->monoLabel->minimumHeight() * UI_SCALE);
    ui->stereoLabel->setMinimumHeight(ui->stereoLabel->minimumHeight() * UI_SCALE);

    // Volume and balance sliders and buttons container
    ui->horizontalWidget_2->setMinimumHeight(ui->horizontalWidget_2->minimumHeight() * UI_SCALE);
    ui->horizontalWidget_2->setMaximumHeight(ui->horizontalWidget_2->maximumHeight() * UI_SCALE);
    ui->horizontalWidget_2->layout()->setContentsMargins(ui->horizontalWidget_2->layout()->contentsMargins() * UI_SCALE);

    ui->eqButton->setMinimumSize(ui->eqButton->minimumSize() * UI_SCALE);
    ui->eqButton->setMaximumSize(ui->eqButton->maximumSize() * UI_SCALE);
    ui->eqButton->setStyleSheet(getStylesheet("playerview.eqButton"));

    ui->playlistButton->setMinimumSize(ui->playlistButton->minimumSize() * UI_SCALE);
    ui->playlistButton->setMaximumSize(ui->playlistButton->maximumSize() * UI_SCALE);
    ui->playlistButton->setStyleSheet(getStylesheet("playerview.playlistButton"));

    ui->balanceSlider->setMinimumSize(ui->balanceSlider->minimumSize() * UI_SCALE);
    ui->balanceSlider->setStyleSheet(getStylesheet("playerview.balanceSlider"));

    ui->volumeSlider->setMinimumSize(ui->volumeSlider->minimumSize() * UI_SCALE);
    ui->volumeSlider->setMaximumSize(ui->volumeSlider->maximumSize() * UI_SCALE);
    ui->volumeSlider->setStyleSheet(getStylesheet("playerview.volumeSlider"));

    ui->songInfoContainer->setMinimumHeight(ui->songInfoContainer->minimumHeight() * UI_SCALE);
    ui->songInfoContainer->setMaximumHeight(ui->songInfoContainer->maximumHeight() * UI_SCALE);
    ui->songInfoContainer->layout()->setContentsMargins(ui->songInfoContainer->layout()->contentsMargins() * UI_SCALE);
    ui->songInfoContainer->setStyleSheet(getStylesheet("playerview.songInfoContainer"));

    ui->visualizationFrame->setMaximumSize(ui->visualizationFrame->maximumSize() * UI_SCALE);
    ui->visualizationFrame->setMinimumSize(ui->visualizationFrame->minimumSize() * UI_SCALE);
    ui->visualizationFrame->setContentsMargins(UI_SCALE, UI_SCALE, UI_SCALE, UI_SCALE);
    ui->visualizationFrame->setStyleSheet(QStringLiteral("#visualizationFrame { background: transparent; border: none; }"));
    ui->visualizationBackground->setStyleSheet(QStringLiteral("#visualizationBackground { background: transparent; border: none; border-image: none; }"));

    ui->playStatusIcon->setMaximumSize(ui->playStatusIcon->maximumSize() * UI_SCALE);
    ui->playStatusIcon->setMinimumSize(ui->playStatusIcon->minimumSize() * UI_SCALE);
    QRect psiGeo = ui->playStatusIcon->geometry();
    ui->playStatusIcon->setGeometry(psiGeo.x()*UI_SCALE,
                                    psiGeo.y()*UI_SCALE,
                                    psiGeo.width(), psiGeo.height());

    ui->progressTimeLabel->setGeometry(36 * UI_SCALE, 3 * UI_SCALE,
                                       51 * UI_SCALE, 13 * UI_SCALE);
    ui->progressTimeLabel->setStyleSheet(QStringLiteral("background: transparent; border: none; padding: 0px; margin: 0px;"));
    ui->progressTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QRect scGeo = ui->spectrumContainer->geometry();
    ui->spectrumContainer->setGeometry(scGeo.x()*UI_SCALE,
                                        scGeo.y()*UI_SCALE + UI_SCALE,
                                        scGeo.width()*UI_SCALE, scGeo.height()*UI_SCALE);

    ui->inputLabel->hide();
    ui->kHzLabel->hide();
}

void PlayerView::setDeferredSeekEnabled(bool enabled)
{
    m_deferredSeekEnabled = enabled;
    if (!enabled) {
        m_isUserSeeking = false;
    }
}

void PlayerView::setPlaybackState(MediaPlayer::PlaybackState state)
{
    m_playbackState = state;
    emit playbackStateChanged(state);
    if (state == MediaPlayer::PausedState) {
        m_pauseTimeVisible = true;
        ui->progressTimeLabel->setVisible(true);
        m_pauseBlinkTimer.start();
    } else {
        m_pauseBlinkTimer.stop();
        m_pauseTimeVisible = true;
        ui->progressTimeLabel->setVisible(true);
    }
    m_pauseDash->setVisible(state != MediaPlayer::StoppedState
                            && (state == MediaPlayer::PausedState || m_timeLeftMode));
    updatePositionSliderStyle();
    if (state != MediaPlayer::StoppedState)
        updateDurationInfo(m_currentPositionSeconds);
    setChannelIndicators(m_monoPlayback, state != MediaPlayer::StoppedState);
    switch(state) {
    case MediaPlayer::StoppedState:
        if(spectrum) spectrum->stop();
        ui->progressTimeLabel->setPixmap(QPixmap());
        ui->progressTimeLabel->setText("");
        break;
    case MediaPlayer::PlayingState:
        if(spectrum) spectrum->play();
        break;
    case MediaPlayer::PausedState:
        if(spectrum) spectrum->pause();
        break;
    }

    // The playback backend can change the global audio value when it becomes
    // active. Keep the built-in Classic volume and balance grooves painted
    // from their skin styles as Winamp does; their value only moves the knobs.
    if (!WinampSkin::instance().isLoaded()) {
        ui->volumeSlider->setStyleSheet(
            WinampSkin::instance().applyToStylesheet(getStylesheet("playerview.volumeSlider")));
        ui->balanceSlider->setStyleSheet(
            WinampSkin::instance().applyToStylesheet(getStylesheet("playerview.balanceSlider")));
        ui->volumeSlider->repaint();
        ui->balanceSlider->repaint();
    }

    // Match Winamp's PLAYPAUS.BMP composition: a 9x9 transport glyph plus
    // the 3x9 green/red status marker immediately to its left while playing.
    updatePlaybackStatusIcon();
}

void PlayerView::updatePlaybackStatusIcon()
{
    const QImage sprite = WinampSkin::instance().sheet(QStringLiteral("playpaus"));
    if (sprite.width() >= 42 && sprite.height() >= 9) {
        int iconOffset = 18;
        int markerOffset = 27;
        int markerWidth = 2;
        if (m_playbackState == MediaPlayer::PlayingState) {
            iconOffset = 0;
            markerOffset = 36;
            markerWidth = 3;
        } else if (m_playbackState == MediaPlayer::PausedState) {
            iconOffset = 9;
        }

        // Winamp's original masked blit leaves the spectrum grid visible
        // through black pixels in PLAYPAUS.BMP. Preserve that with alpha.
        QImage image(11, 9, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        const QRgb bgKey = sprite.pixel(0, 0) & 0x00ffffff;
        auto copyMasked = [&sprite, &image, bgKey](int sourceX, int targetX, int width) {
            for (int y = 0; y < 9; ++y) {
                for (int x = 0; x < width; ++x) {
                    if ((sprite.pixel(sourceX + x, y) & 0x00ffffff) != bgKey)
                        image.setPixelColor(targetX + x, y, sprite.pixelColor(sourceX + x, y));
                }
            }
        };
        copyMasked(iconOffset, 2, 9);
        copyMasked(markerOffset, 0, markerWidth);

        QGraphicsScene *scene = ui->playStatusIcon->scene();
        if (!scene) {
            scene = new QGraphicsScene(this);
            ui->playStatusIcon->setScene(scene);
        }
        scene->clear();
        scene->addPixmap(QPixmap::fromImage(image));
        scene->setSceneRect(image.rect());
        ui->playStatusIcon->raise();
    }
}

static QPixmap renderSkinBitmapText(const QString &text, int fieldChars)
{
    if (text.isEmpty() || fieldChars <= 0)
        return QPixmap();

    QImage fontImage = WinampSkin::instance().sheet(QStringLiteral("text"));
    if (fontImage.isNull())
        return QPixmap();

    if (!fontImage.hasAlphaChannel()) {
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

    QImage out(fieldChars * 5, 6, QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    QPainter p(&out);
    const QString padded = text.right(fieldChars).rightJustified(fieldChars, QLatin1Char(' '));
    for (int i = 0; i < fieldChars; ++i) {
        const QChar ch = padded[i];
        int row = -1;
        int col = -1;
        if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
            row = 1;
            col = ch.unicode() - '0';
        } else if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) {
            row = 0;
            col = ch.unicode() - 'A';
        } else if (ch >= QLatin1Char('a') && ch <= QLatin1Char('z')) {
            row = 0;
            col = ch.unicode() - 'a';
        }
        if (row >= 0 && col >= 0 && fontImage.rect().contains(QRect(col * 5, row * 6, 5, 6)))
            p.drawImage(i * 5, 0, fontImage, col * 5, row * 6, 5, 6);
    }
    p.end();
    if (UI_SCALE > 1)
        out = out.scaled(out.size() * UI_SCALE, Qt::KeepAspectRatio, Qt::FastTransformation);
    return QPixmap::fromImage(out);
}

void PlayerView::updateRateLabels()
{
    const QPixmap kbpsPix = renderSkinBitmapText(m_kbpsText, 3);
    if (!kbpsPix.isNull() || m_kbpsText.isEmpty()) {
        ui->kbpsValueLabel->setText(QString());
        ui->kbpsValueLabel->setPixmap(kbpsPix);
    } else {
        ui->kbpsValueLabel->setPixmap(QPixmap());
        ui->kbpsValueLabel->setText(m_kbpsText);
    }

    const QPixmap khzPix = renderSkinBitmapText(m_khzText, 2);
    if (!khzPix.isNull() || m_khzText.isEmpty()) {
        ui->khzValueLabel->setText(QString());
        ui->khzValueLabel->setPixmap(khzPix);
    } else {
        ui->khzValueLabel->setPixmap(QPixmap());
        ui->khzValueLabel->setText(m_khzText);
    }

    ui->kbpsValueLabel->show();
    ui->khzValueLabel->show();
    ui->kbpsValueLabel->raise();
    ui->khzValueLabel->raise();
    ui->kbpsValueLabel->update();
    ui->khzValueLabel->update();
}

void PlayerView::updatePositionSliderStyle()
{
    const QString base = getStylesheet(QStringLiteral("playerview.posBar"));
    QString stylesheet = WinampSkin::instance().applyToStylesheet(base);
    if (m_playbackState == MediaPlayer::StoppedState) {
        stylesheet += QStringLiteral(
            "\nQSlider::handle:horizontal {"
            " width: 0px; height: 0px; border: 0;"
            " border-image: none; background: transparent;"
            " margin: 0px; }\n");
    }
    ui->posBar->setProperty("_linkampBaseStyleSheet", base);
    ui->posBar->setStyleSheet(stylesheet);
}

void PlayerView::setPosition(qint64 progress)
{
    emit playbackPositionChanged(progress);

    if (m_deferredSeekEnabled) {
        if (m_isUserSeeking || ui->posBar->isSliderDown()) {
            return;
        }

        ui->posBar->setValue(progress);
        updateDurationInfo(progress / 1000);
        return;
    }

    if (!ui->posBar->isSliderDown()) {
        ui->posBar->setValue(progress);
    }

    updateDurationInfo(progress / 1000);
}

void PlayerView::setSpectrumData(const QByteArray& data, QAudioFormat format)
{
    m_audioSampleRateHz = format.sampleRate();
    if (m_audioSampleRateHz > 0 && m_khzText.isEmpty()) {
        m_khzText = QString::number(m_audioSampleRateHz / 1000);
        updateRateLabels();
    }
    spectrum->setData(data, format);
    emit audioDataAvailable(data, format);
}

void PlayerView::setMetadata(QMediaMetaData metadata)
{
    // Generate track info string
    const QString title = repairUtf8Mojibake(
        metadata.value(QMediaMetaData::Title).toString());
    emit currentTrackTitleChanged(title.trimmed());

    //  Calculate duration
    qint64 ms = metadata.value(QMediaMetaData::Duration).toLongLong();
    qint64 duration = ms/1000;
    QString durationStr = formatDuration(ms);

    QString trackInfo;
    const int trackNumber = metadata.value(QMediaMetaData::TrackNumber).toInt();
    if (trackNumber > 0)
        trackInfo.append(QStringLiteral("%1. ").arg(trackNumber));

    if (!title.isEmpty())
        trackInfo.append(title);
    if (duration > 0)
        trackInfo.append(QStringLiteral(" (%1)").arg(durationStr));

    setTrackInfo(trackInfo);

    // Set kbps
    int bitrate = metadata.value(QMediaMetaData::AudioBitRate).toInt();
    QString codec = metadata.value(QMediaMetaData::Description).toString(); // Using Description as codec
    // FFmpeg may report a transient MPEG frame rate such as 4 kb/s. It is
    // not the stream bitrate and Winamp does not display that as a valid rate.
    if(bitrate >= 8000) {
        bitrate = bitrate/1000;
        m_kbpsText = QString::number(bitrate);
    } else if(codec.length()) {
        m_kbpsText = codec;
    } else {
        m_kbpsText.clear();
    }

    // Set kHz
    bool sampleRateOk = false;
    int sampleRate = metadata.value(QMediaMetaData::Comment).toString().toInt(&sampleRateOk);
    if ((!sampleRateOk || sampleRate <= 0) && m_audioSampleRateHz > 0)
        sampleRate = m_audioSampleRateHz;
    const int khz = sampleRate / 1000;
    m_khzText = khz > 0 ? QString::number(khz) : QString();
    updateRateLabels();
}

void PlayerView::setChannelIndicators(bool mono, bool active)
{
    m_monoPlayback = mono;
    const QImage sheet = WinampSkin::instance().sheet(QStringLiteral("monoster"));
    if (sheet.isNull() || sheet.width() < 56 || sheet.height() < 24) {
        m_monoIndicator->setText(mono ? QStringLiteral("mono") : QString());
        m_stereoIndicator->setText(mono ? QString() : QStringLiteral("stereo"));
        return;
    }

    const QRect monoRect(29, mono && active ? 0 : 12, 27, 12);
    const QRect stereoRect(0, !mono && active ? 0 : 12, 29, 12);
    m_monoIndicator->setPixmap(QPixmap::fromImage(sheet.copy(monoRect)));
    m_stereoIndicator->setPixmap(QPixmap::fromImage(sheet.copy(stereoRect)));
}

void PlayerView::setDuration(qint64 duration)
{
    m_duration = duration / 1000;
    ui->posBar->setMaximum(duration);
    ui->posBar->setVisible(true);
    ui->volumeSlider->repaint();
    ui->balanceSlider->repaint();
}

void PlayerView::refreshSkin()
{
    // Skin refreshes replace the slider's sprite URLs. Reapply its canonical
    // stylesheet and visibility so switching back to Classic restores it too.
    updatePositionSliderStyle();
    ui->posBar->setVisible(true);
    ui->posBar->updateGeometry();
    ui->posBar->update();
    updatePlaybackStatusIcon();
    setChannelIndicators(m_monoPlayback, m_playbackState != MediaPlayer::StoppedState);
    updateRateLabels();
    updateDurationInfo(m_currentPositionSeconds);
}

void PlayerView::setVolume(int volume)
{
    if (!ui->volumeSlider->isSliderDown()) {
        ui->volumeSlider->setValue(volume);
    }
}

void PlayerView::setBalance(int balance)
{
    if (!ui->balanceSlider->isSliderDown()) {
        ui->balanceSlider->setValue(balance);
    }
}

void PlayerView::setEqEnabled(bool enabled)
{
    eqEnabled = enabled;
    ui->eqButton->setChecked(eqEnabled);
}

void PlayerView::setPlEnabled(bool enabled)
{
    plEnabled = enabled;
    ui->playlistButton->setChecked(plEnabled);
}

void PlayerView::setShuffleEnabled(bool enabled)
{
    shuffleEnabled = enabled;
    controlButtons->setShuffleEnabled(shuffleEnabled);
}

void PlayerView::setRepeatEnabled(bool enabled)
{
    repeatEnabled = enabled;
    controlButtons->setRepeatEnabled(repeatEnabled);
}

void PlayerView::setMessage(QString message, qint64 timeout)
{
    ui->songInfoLabel->setText(message);
    messageTimer->setSingleShot(true);
    messageTimer->start(timeout);
}

void PlayerView::setPersistentMessage(const QString &message)
{
    messageTimer->stop();
    ui->songInfoLabel->setText(message);
}

void PlayerView::clearMessage()
{
    if(ui->songInfoLabel->text() != m_trackInfo) {
        ui->songInfoLabel->setText(m_trackInfo);
    }
    if(messageTimer->isActive()) {
        messageTimer->stop();
    }
}

void PlayerView::updateDurationInfo(qint64 currentInfo)
{
    m_currentPositionSeconds = std::max<qint64>(0, currentInfo);
    if (m_playbackState == MediaPlayer::StoppedState) {
        ui->progressTimeLabel->setPixmap(QPixmap());
        ui->progressTimeLabel->setText("");
        return;
    }
    const qint64 seconds = m_timeLeftMode
        ? std::max<qint64>(0, m_duration - m_currentPositionSeconds)
        : m_currentPositionSeconds;
    const QString minutes = QString::number(seconds / 60).rightJustified(2, QLatin1Char('0'));
    const QString remainder = QString::number(seconds % 60).rightJustified(2, QLatin1Char('0'));

    QImage numbersSheet = WinampSkin::instance().sheet(QStringLiteral("numbers"));
    if (!numbersSheet.isNull() && numbersSheet.width() >= 90 && numbersSheet.height() >= 13) {
        const QColor bgColor = numbersSheet.pixelColor(0, 0);
        QColor dashColor = numbersSheet.pixelColor(22, 6);
        int bestDist = -1;
        for (int y = 0; y < 13; ++y) {
            for (int x = 0; x < 90; ++x) {
                const QColor c = numbersSheet.pixelColor(x, y);
                const int dist = qAbs(c.red() - bgColor.red())
                               + qAbs(c.green() - bgColor.green())
                               + qAbs(c.blue() - bgColor.blue());
                if (dist > bestDist) {
                    bestDist = dist;
                    dashColor = c;
                }
            }
        }
        if (dashColor.isValid() && bestDist > 0) {
            m_pauseDash->setStyleSheet(
                QStringLiteral("background-color: %1; border: none;").arg(dashColor.name()));
        }
        QImage out(51, 13, QImage::Format_ARGB32);
        out.fill(Qt::transparent);
        QPainter p(&out);
        const int digits[4] = {
            minutes[0].unicode() - '0',
            minutes[1].unicode() - '0',
            remainder[0].unicode() - '0',
            remainder[1].unicode() - '0'
        };
        const int xOffsets[4] = {0, 12, 30, 42};
        for (int i = 0; i < 4; ++i) {
            const int d = std::clamp(digits[i], 0, 9);
            p.drawImage(xOffsets[i], 0, numbersSheet, d * 9, 0, 9, 13);
        }
        p.end();
        if (UI_SCALE > 1)
            out = out.scaled(out.size() * UI_SCALE, Qt::KeepAspectRatio, Qt::FastTransformation);
        ui->progressTimeLabel->setText(QString());
        ui->progressTimeLabel->setPixmap(QPixmap::fromImage(out));
        return;
    }

    const QString tDisplayStr = minutes + QLatin1Char(':') + remainder;
    ui->progressTimeLabel->setPixmap(QPixmap());
    ui->progressTimeLabel->setText(tDisplayStr);
}

bool PlayerView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->posBar && event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        QSlider *slider = ui->posBar;
        if (mouseEvent->button() == Qt::LeftButton && slider->maximum() > slider->minimum()) {
            QStyleOptionSlider option;
            option.initFrom(slider);
            option.orientation = slider->orientation();
            option.minimum = slider->minimum();
            option.maximum = slider->maximum();
            option.sliderPosition = slider->sliderPosition();
            option.sliderValue = slider->value();
            option.singleStep = slider->singleStep();
            option.pageStep = slider->pageStep();
            option.upsideDown = slider->invertedAppearance();
            const QRect handle = slider->style()->subControlRect(
                QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider);
            if (!handle.contains(mouseEvent->pos())) {
                const int span = std::max(1, slider->width() - handle.width());
                const int position = std::clamp(
                    mouseEvent->pos().x() - handle.width() / 2, 0, span);
                const int value = QStyle::sliderValueFromPosition(
                    slider->minimum(), slider->maximum(), position, span,
                    option.upsideDown);
                slider->setValue(value);
                if (m_deferredSeekEnabled)
                    updateDurationInfo(value / 1000);
                emit positionChanged(value);
                return true;
            }
        }
    }

    if (watched == ui->progressTimeLabel && event->type() == QEvent::MouseButtonRelease) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_timeLeftMode = !m_timeLeftMode;
            QSettings().setValue(QStringLiteral("player/timeLeftMode"), m_timeLeftMode);
            m_pauseDash->setVisible(m_playbackState != MediaPlayer::StoppedState
                                    && (m_playbackState == MediaPlayer::PausedState
                                        || m_timeLeftMode));
            updateDurationInfo(m_currentPositionSeconds);
            return true;
        }
    }
    if (watched == window() && (event->type() == QEvent::ActivationChange
                                || event->type() == QEvent::WindowActivate
                                || event->type() == QEvent::WindowDeactivate)) {
        if (auto *label = findChild<QLabel *>(QStringLiteral("classicVisualModeLabels"))) {
            QImage atlas = WinampSkin::instance().sheet(QStringLiteral("titlebar"));
            if (atlas.isNull() || atlas.width() < 320 || atlas.height() < 43)
                atlas = WinampSkin::instance().sheet(QStringLiteral("titlebar-atlas"));
            const int srcX = (window() && window()->isActiveWindow()) ? 304 : 312;
            if (!atlas.isNull() && atlas.width() >= srcX + 8 && atlas.height() >= 43)
                label->setPixmap(QPixmap::fromImage(atlas.copy(srcX, 0, 8, 43)));
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PlayerView::setTrackInfo(const QString &info)
{
    m_trackInfo = info;

    // Don't override info label if message is in flight
    if(!messageTimer->isActive()) {
        ui->songInfoLabel->setText(info);
    }

    if (!m_statusInfo.isEmpty())
        setWindowTitle(QString("%1 | %2").arg(m_trackInfo, m_statusInfo));
    else
        setWindowTitle(m_trackInfo);
}

void PlayerView::handleBalanceChanged()
{
    // Snap slider to the center if it falls near to it
    int val = ui->balanceSlider->value();
    if(val > -20 && val < 20) {
        val = 0;
        ui->balanceSlider->setValue(val);
    }
    emit balanceChanged(val);
}

void PlayerView::setSourceLabel(QString label)
{
    QString fLabel = "";
    for(quint8 i = 0; i < label.length(); i++) {
        fLabel.append(label[i]);
        fLabel.append("\n");
    }
    ui->inputLabel->setText(fLabel);
}
