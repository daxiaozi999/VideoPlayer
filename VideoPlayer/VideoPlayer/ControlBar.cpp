#include "ControlBar.h"

ControlBar::ControlBar(QWidget* parent)
    : QFrame(parent)
    , mainLayout(nullptr)
    , controlsLayout(nullptr)
    , leftWidget(nullptr)
    , leftLayout(nullptr)
    , rightWidget(nullptr)
    , rightLayout(nullptr)
    , progressSlider(nullptr)
    , settingsButton(nullptr)
    , playButton(nullptr)
    , pauseButton(nullptr)
    , timeLabel(nullptr)
    , speedComboBox(nullptr)
    , volumeButton(nullptr)
    , muteButton(nullptr)
    , volumeSlider(nullptr)
    , fullscreenButton(nullptr) {

    setupUi();
    if (validateComponents()) {
        connectSignals();
    }
}

int ControlBar::getCurrentProgress() const {
    return progressSlider ? progressSlider->value() : 0;
}

int ControlBar::getCurrentVolume() const {
    return volumeSlider ? volumeSlider->value() : 0;
}

float ControlBar::getCurrentSpeed() const {
    if (!speedComboBox) return 1.0f;

    const QString speedText = speedComboBox->currentText();
    return parseSpeedText(speedText);
}

void ControlBar::setProgressSliderEnabled(bool enable) {
    if (progressSlider) {
        progressSlider->setEnabled(enable);
        progressSlider->setHidden(!enable);
    }
}

void ControlBar::setSpeedComboBoxEnabled(bool enable) {
    if (speedComboBox) {
        speedComboBox->setEnabled(enable);
        speedComboBox->setHidden(!enable);
    }
}

void ControlBar::setPlayButtonVisible(bool visible) {
    if (playButton) {
        playButton->setVisible(visible);
    }
}

void ControlBar::setPauseButtonVisible(bool visible) {
    if (pauseButton) {
        pauseButton->setVisible(visible);
    }
}

void ControlBar::setVolumeButtonVisible(bool visible) {
    if (volumeButton) {
        volumeButton->setVisible(visible);
    }
}

void ControlBar::setMuteButtonVisible(bool visible) {
    if (muteButton) {
        muteButton->setVisible(visible);
    }
}

void ControlBar::setTimeLabel(const QString& currentTime, const QString& totalTime) {
    if (!timeLabel) return;

    if (totalTime.isEmpty()) {
        timeLabel->setText(currentTime);
    }
    else {
        timeLabel->setText(currentTime + " / " + totalTime);
    }
}

void ControlBar::setProgress(int value) {
    if (progressSlider) {
        progressSlider->setValue(value);
    }
}

void ControlBar::setProgressRange(int min, int max) {
    if (progressSlider) {
        progressSlider->setRange(min, max);
    }
}

void ControlBar::setPlaybackSpeed(float speed) {
    if (!speedComboBox) return;

    const QString speedText = formatSpeedText(speed);
    const int index = speedComboBox->findText(speedText);

    speedComboBox->blockSignals(true);
    if (index >= 0) {
        speedComboBox->setCurrentIndex(index);
    }
    else {
        const int defaultIndex = speedComboBox->findText("1.0x");
        speedComboBox->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 1);
    }
    speedComboBox->blockSignals(false);
}

void ControlBar::setVolume(int value) {
    if (volumeSlider) {
        volumeSlider->setValue(qBound(0, value, 100));
    }
}

void ControlBar::setFullscreen(bool isFullscreen) {
    if (!fullscreenButton) return;

    if (isFullscreen) {
        fullscreenButton->setToolTip("Exit Fullscreen(F)");
    }
    else {
        fullscreenButton->setToolTip("Enter Fullscreen(F)");
    }
}

void ControlBar::onSettingsClicked() {
    emit settingsClicked();
}

void ControlBar::onPlayClicked() {
    emit playClicked();
}

void ControlBar::onPauseClicked() {
    emit pauseClicked();
}

void ControlBar::onVolumeButtonClicked() {
    emit volumeButtonClicked();
}

void ControlBar::onMuteButtonClicked() {
    emit muteButtonClicked();
}

void ControlBar::onVolumeSliderClicked(int value) {
    emit volumeSliderClicked(value);
}

void ControlBar::onVolumeSliderMoved(int value) {
    emit volumeSliderMoved(value);
}

void ControlBar::onVolumeSliderReleased(int value) {
    emit volumeSliderReleased(value);
}

void ControlBar::onProgressSliderClicked(int value) {
    emit progressSliderClicked(value);
}

void ControlBar::onProgressSliderMoved(int value) {
    emit progressSliderMoved(value);
}

void ControlBar::onProgressSliderReleased(int value) {
    emit progressSliderReleased(value);
}

void ControlBar::onSpeedChanged(int index) {
    if (index < 0 || !speedComboBox) return;

    const QString speedText = speedComboBox->itemText(index);
    const float speed = parseSpeedText(speedText);
    if (speed > 0) {
        emit speedChanged(speed);
    }
}

void ControlBar::onFullscreenClicked() {
    emit fullscreenClicked();
}

void ControlBar::setupUi() {
    setControlBarStyle();
    createMainLayout();
    setupProgressSlider();
    setupControlsLayout();
    setupLeftControls();
    setupRightControls();
    addWidgetsToLayout();
}

bool ControlBar::validateComponents() const {
    return progressSlider && mainLayout && controlsLayout &&
           leftWidget && leftLayout && rightWidget && rightLayout &&
           settingsButton && playButton && pauseButton && timeLabel &&
           speedComboBox && volumeButton && muteButton &&
           volumeSlider && fullscreenButton;
}

void ControlBar::connectSignals() {
    if (progressSlider) {
        connect(progressSlider, &CustomSlider::sliderClicked, this, &ControlBar::onProgressSliderClicked);
        connect(progressSlider, &CustomSlider::sliderMoved, this, &ControlBar::onProgressSliderMoved);
        connect(progressSlider, &CustomSlider::sliderReleased, this, &ControlBar::onProgressSliderReleased);
    }

    if (volumeSlider) {
        connect(volumeSlider, &CustomSlider::sliderClicked, this, &ControlBar::onVolumeSliderClicked);
        connect(volumeSlider, &CustomSlider::sliderMoved, this, &ControlBar::onVolumeSliderMoved);
        connect(volumeSlider, &CustomSlider::sliderReleased, this, &ControlBar::onVolumeSliderReleased);
    }

    if (volumeButton) {
        connect(volumeButton, &QPushButton::clicked, this, &ControlBar::onVolumeButtonClicked);
    }

    if (muteButton) {
        connect(muteButton, &QPushButton::clicked, this, &ControlBar::onMuteButtonClicked);
    }

    if (settingsButton) {
        connect(settingsButton, &QPushButton::clicked, this, &ControlBar::onSettingsClicked);
    }

    if (playButton) {
        connect(playButton, &QPushButton::clicked, this, &ControlBar::onPlayClicked);
    }

    if (pauseButton) {
        connect(pauseButton, &QPushButton::clicked, this, &ControlBar::onPauseClicked);
    }

    if (fullscreenButton) {
        connect(fullscreenButton, &QPushButton::clicked, this, &ControlBar::onFullscreenClicked);
    }

    if (speedComboBox) {
        connect(speedComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlBar::onSpeedChanged);
    }
}

void ControlBar::setControlBarStyle() {
    setStyleSheet("ControlBar { background: transparent; }");
    setFixedHeight(100);
}

void ControlBar::createMainLayout() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 5, 15, 5);
    mainLayout->setSpacing(5);
}

void ControlBar::setupProgressSlider() {
    progressSlider = new CustomSlider(Qt::Horizontal, this);
    if (!progressSlider) return;

    progressSlider->setRange(0, 100);
    progressSlider->setValue(0);
    progressSlider->setFixedHeight(24);
    progressSlider->setHandleSize(17);
    progressSlider->setGrooveHeight(7);
    progressSlider->setWheelEnabled(false);

    progressSlider->setProgressColor(QColor(39, 174, 96));
    progressSlider->setHoverColor(QColor(46, 204, 113));
    progressSlider->setPressedColor(QColor(22, 160, 133));
    progressSlider->setBackgroundColor(QColor(255, 255, 255, 40));
}

void ControlBar::setupControlsLayout() {
    controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(15);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
}

void ControlBar::setupLeftControls() {
    createLeftWidget();
    createLeftButtons();
    createTimeLabel();
    addLeftWidgetsToLayout();
}

void ControlBar::setupRightControls() {
    createRightWidget();
    createRightButtons();
    createSpeedComboBox();
    createVolumeSlider();
    addRightWidgetsToLayout();
}

void ControlBar::addWidgetsToLayout() {
    if (mainLayout && progressSlider) {
        mainLayout->addWidget(progressSlider);
    }
    if (mainLayout && controlsLayout) {
        mainLayout->addLayout(controlsLayout);
    }
    if (controlsLayout && leftWidget) {
        controlsLayout->addWidget(leftWidget);
    }
    if (controlsLayout && rightWidget) {
        controlsLayout->addWidget(rightWidget);
    }
}

void ControlBar::createLeftWidget() {
    leftWidget = new QWidget(this);
    if (!leftWidget) return;

    leftWidget->setStyleSheet("background: transparent;");
    leftLayout = new QHBoxLayout(leftWidget);
    if (leftLayout) {
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(12);
    }
}

void ControlBar::createLeftButtons() {
    settingsButton = createControlButton(":/VideoPlayer/icons/setting.png", "Settings(S)");
    playButton = createControlButton(":/VideoPlayer/icons/play.png", "Play(Space)");
    pauseButton = createControlButton(":/VideoPlayer/icons/pause.png", "Pause(Space)");

    if (playButton) {
        playButton->setMinimumSize(45, 45);
    }
    if (pauseButton) {
        pauseButton->setMinimumSize(45, 45);
        pauseButton->hide();
    }
}

void ControlBar::createTimeLabel() {
    timeLabel = new QLabel("00:00 / 00:00", this);
    if (!timeLabel) return;

    timeLabel->setStyleSheet(R"(
        QLabel {
            color: #ffffff;
            font-size: 22px;
            font-weight: bold;
            background: transparent;
            border: none;
            min-width: 140px;
            max-height: 60px;
            min-height: 60px;
            padding: 0px 8px;
        }
    )");
    timeLabel->setAlignment(Qt::AlignCenter);
}

void ControlBar::addLeftWidgetsToLayout() {
    if (!leftLayout) return;

    if (settingsButton) leftLayout->addWidget(settingsButton);
    if (playButton) leftLayout->addWidget(playButton);
    if (pauseButton) leftLayout->addWidget(pauseButton);
    if (timeLabel) leftLayout->addWidget(timeLabel);
    leftLayout->addStretch(1);
}

void ControlBar::createRightWidget() {
    rightWidget = new QWidget(this);
    if (!rightWidget) return;

    rightWidget->setStyleSheet("background: transparent;");
    rightLayout = new QHBoxLayout(rightWidget);
    if (rightLayout) {
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(12);
        rightLayout->addStretch();
    }
}

void ControlBar::createRightButtons() {
    volumeButton = createControlButton(":/VideoPlayer/icons/volume.png", "Volume(+/-)");
    muteButton = createControlButton(":/VideoPlayer/icons/mute.png", "Mute(M)");
    fullscreenButton = createControlButton(":/VideoPlayer/icons/fullscreen.png", "Enter Fullscreen(F)");

    if (volumeButton) {
        volumeButton->setMinimumSize(45, 45);
    }
    if (muteButton) {
        muteButton->setMinimumSize(45, 45);
        muteButton->hide();
    }
    if (fullscreenButton) {
        fullscreenButton->setMinimumSize(45, 45);
    }
}

void ControlBar::createSpeedComboBox() {
    speedComboBox = new QComboBox(this);
    if (!speedComboBox) return;

    speedComboBox->addItems({ "0.5x", "1.0x", "1.5x", "2.0x" });
    speedComboBox->setCurrentText("1.0x");
    speedComboBox->setCursor(Qt::PointingHandCursor);
    speedComboBox->setStyleSheet(getSpeedComboBoxStyle());
    speedComboBox->setFixedSize(70, 45);
}

void ControlBar::createVolumeSlider() {
    volumeSlider = new CustomSlider(Qt::Horizontal, this);
    if (!volumeSlider) return;

    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(70);
    volumeSlider->setFixedHeight(24);
    volumeSlider->setFixedWidth(100);
    volumeSlider->setHandleSize(17);
    volumeSlider->setGrooveHeight(7);
    volumeSlider->setWheelEnabled(false);

    volumeSlider->setProgressColor(QColor(241, 196, 15));
    volumeSlider->setHoverColor(QColor(243, 156, 18));
    volumeSlider->setPressedColor(QColor(212, 120, 7));
    volumeSlider->setBackgroundColor(QColor(255, 255, 255, 40));
}

void ControlBar::addRightWidgetsToLayout() {
    if (!rightLayout) return;

    if (speedComboBox) rightLayout->addWidget(speedComboBox);
    if (volumeButton) rightLayout->addWidget(volumeButton);
    if (muteButton) rightLayout->addWidget(muteButton);
    if (volumeSlider) rightLayout->addWidget(volumeSlider);
    if (fullscreenButton) rightLayout->addWidget(fullscreenButton);
}

QPushButton* ControlBar::createControlButton(const QString& iconPath, const QString& tooltip) {
    QPushButton* button = new QPushButton(this);
    if (!button) return nullptr;

    button->setIcon(QIcon(iconPath));
    button->setToolTip(tooltip);
    button->setIconSize(QSize(32, 32));
    button->setFixedSize(45, 45);
    button->setStyleSheet(getControlButtonStyle());
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QString ControlBar::getControlButtonStyle() const {
    return R"(
        QPushButton {
            background: transparent;
            border: none;
            padding: 0px;
            margin: 0px;
            border-radius: 8px;
        }
        QPushButton:hover {
            background: rgba(52, 152, 219, 30);
            border: 2px solid #3498db;
        }
        QPushButton:pressed {
            background: rgba(52, 152, 219, 50);
            border: 2px solid #3498db;
        }
        QPushButton:focus {
            outline: none;
            border: none;
        }
    )";
}

QString ControlBar::getSpeedComboBoxStyle() const {
    return R"(
        QComboBox {
            background: transparent;
            color: #ffffff;
            border: none;
            border-radius: 8px;
            font-size: 22px;
            font-weight: bold;
            min-width: 55px;
            max-width: 55px;
            min-height: 45px;
            max-height: 45px;
            padding: 0px 10px;
            text-align: center;
        }
        QComboBox:hover {
            background: rgba(52, 152, 219, 30);
            border: 2px solid #3498db;
        }
        QComboBox:focus {
            outline: none;
            background: rgba(52, 152, 219, 50);
            border: 2px solid #3498db;
        }
        QComboBox:pressed {
            background: rgba(52, 152, 219, 50);
            border: 2px solid #3498db;
        }
        QComboBox::drop-down {
            border: none;
            width: 0px;
            background: transparent;
        }
        QComboBox::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
        }
        QComboBox QAbstractItemView {
            background-color: #2b2b2b;
            color: #ffffff;
            border: 2px solid #3498db;
            border-radius: 4px;
            selection-background-color: #3498db;
            outline: none;
            font-size: 20px;
            padding: 5px;
        }
        QComboBox QAbstractItemView::item {
            padding: 8px 15px;
            border: none;
            border-radius: 2px;
            text-align: center;
            min-height: 25px;
            background: transparent;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: rgba(52, 152, 219, 100);
        }
        QComboBox QAbstractItemView::item:selected {
            background-color: #3498db;
            color: #ffffff;
        }
    )";
}

float ControlBar::parseSpeedText(const QString& speedText) const {
    QString cleanText = speedText;
    cleanText.remove("x");
    bool ok;
    float speed = cleanText.toFloat(&ok);
    return ok ? speed : 1.0f;
}

QString ControlBar::formatSpeedText(float speed) const {
    return QString::number(speed, 'f', 1) + "x";
}