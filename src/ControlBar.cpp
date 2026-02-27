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
    , menuButton(nullptr)
    , playButton(nullptr)
    , pauseButton(nullptr)
    , stopButton(nullptr)
    , timeLabel(nullptr)
    , speedComboBox(nullptr)
    , volumeButton(nullptr)
    , muteButton(nullptr)
    , volumeSlider(nullptr)
    , fullscreenButton(nullptr) {

    setupUi();
    setupConnections();
}

int ControlBar::getCurrentProgress() const {
    return progressSlider->value();
}

int ControlBar::getCurrentVolume() const {
    return volumeSlider->value();
}

float ControlBar::getCurrentSpeed() const {
    return speedTextToFloat(speedComboBox->currentText());
}

void ControlBar::setProgressSliderEnabled(bool enabled) {
    progressSlider->setEnabled(enabled);
    progressSlider->setVisible(enabled);
}

void ControlBar::setSpeedComboBoxEnabled(bool enabled) {
    speedComboBox->setEnabled(enabled);
    speedComboBox->setVisible(enabled);
}

void ControlBar::setPlayButtonVisible(bool visible) {
    playButton->setVisible(visible);
}

void ControlBar::setPauseButtonVisible(bool visible) {
    pauseButton->setVisible(visible);
}

void ControlBar::setVolumeButtonVisible(bool visible) {
    volumeButton->setVisible(visible);
}

void ControlBar::setMuteButtonVisible(bool visible) {
    muteButton->setVisible(visible);
}

void ControlBar::setTimeLabel(const QString& currentTime, const QString& totalTime) {
    if (totalTime.isEmpty()) {
        timeLabel->setText(currentTime);
    }
    else {
        timeLabel->setText(currentTime + " / " + totalTime);
    }
}

void ControlBar::setProgress(int value) {
    progressSlider->setValue(value);
}

void ControlBar::setSpeed(float speed) {
    const QString speedText = QString::number(speed, 'f', 1) + "X";
    const int index = speedComboBox->findText(speedText);

    speedComboBox->blockSignals(true);
    if (index >= 0) {
        speedComboBox->setCurrentIndex(index);
    }
    else {
        const int defaultIndex = speedComboBox->findText("1.0X");
        speedComboBox->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 1);
    }
    speedComboBox->blockSignals(false);
}

void ControlBar::setVolume(int value) {
    volumeSlider->setValue(qBound(0, value, 100));
}

void ControlBar::setFullscreen(bool isFullscreen) {
    if (isFullscreen) {
        fullscreenButton->setToolTip("Exit Fullscreen(F)");
    }
    else {
        fullscreenButton->setToolTip("Enter Fullscreen(F)");
    }
}

void ControlBar::setupUi() {
    setStyleSheet("ControlBar { background: transparent; }");
    setFixedHeight(100);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 5, 15, 5);
    mainLayout->setSpacing(5);

    setupProgressSlider();
    setupControlsLayout();
    setupLeftControls();
    setupRightControls();

    mainLayout->addWidget(progressSlider);
    mainLayout->addLayout(controlsLayout);
    controlsLayout->addWidget(leftWidget);
    controlsLayout->addWidget(rightWidget);
}

void ControlBar::setupProgressSlider() {
    progressSlider = new CustomSlider(this);
    progressSlider->setRange(0, 100);
    progressSlider->setValue(0);
    progressSlider->setFixedHeight(24);
    progressSlider->setHandleSize(17);
    progressSlider->setGrooveHeight(7);
    progressSlider->setWheelEnabled(false);
    progressSlider->setProgressColor(QColor(39, 174, 96));
}

void ControlBar::setupControlsLayout() {
    controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(15);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
}

void ControlBar::setupLeftControls() {
    leftWidget = new QWidget(this);
    leftWidget->setStyleSheet("background: transparent;");
    leftLayout = new QHBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(12);

    menuButton = createControlButton(":/VideoPlayer/icons/menu.png", "Menu(Ctrl+M)");
    playButton = createControlButton(":/VideoPlayer/icons/play.png", "Play(Space)");
    pauseButton = createControlButton(":/VideoPlayer/icons/pause.png", "Pause(Space)");
    stopButton = createControlButton(":/VideoPlayer/icons/stop.png", "Stop(Ctrl+S)");
    pauseButton->hide();

    timeLabel = new QLabel("00:00 / 00:00", this);
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

    leftLayout->addWidget(menuButton);
    leftLayout->addWidget(playButton);
    leftLayout->addWidget(pauseButton);
    leftLayout->addWidget(stopButton);
    leftLayout->addWidget(timeLabel);
    leftLayout->addStretch(1);
}

void ControlBar::setupRightControls() {
    rightWidget = new QWidget(this);
    rightWidget->setStyleSheet("background: transparent;");
    rightLayout = new QHBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    speedComboBox = new QComboBox(this);
    speedComboBox->addItems({ "0.5X", "1.0X", "1.5X", "2.0X" });
    speedComboBox->setCurrentText("1.0X");
    speedComboBox->setStyleSheet(getSpeedComboBoxStyle());
    speedComboBox->setFixedSize(70, 45);
    speedComboBox->setFocusPolicy(Qt::NoFocus);

    QFont font;
    font.setBold(true);
    font.setPointSize(12);

    volumeButton = createControlButton(":/VideoPlayer/icons/volume.png", "Volume(+/-)");
    muteButton = createControlButton(":/VideoPlayer/icons/mute.png", "Mute(M)");
    fullscreenButton = createControlButton(":/VideoPlayer/icons/fullscreen.png", "Enter Fullscreen(F)");
    muteButton->hide();

    volumeSlider = new CustomSlider(this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(70);
    volumeSlider->setFixedHeight(24);
    volumeSlider->setFixedWidth(120);
    volumeSlider->setHandleSize(17);
    volumeSlider->setGrooveHeight(7);
    volumeSlider->setWheelEnabled(true);
    volumeSlider->setProgressColor(QColor(241, 196, 15));

    rightLayout->addWidget(speedComboBox);
    rightLayout->addWidget(volumeButton);
    rightLayout->addWidget(muteButton);
    rightLayout->addWidget(volumeSlider);
    rightLayout->addWidget(fullscreenButton);
    rightLayout->setSizeConstraint(QLayout::SetFixedSize);
}

void ControlBar::setupConnections() {
    connect(progressSlider, &CustomSlider::sliderPressed, this, &ControlBar::progressSliderPressed);
    connect(progressSlider, &CustomSlider::sliderMoved, this, &ControlBar::progressSliderMoved);
    connect(progressSlider, &CustomSlider::sliderReleased, this, &ControlBar::progressSliderReleased);
    connect(volumeSlider, &CustomSlider::sliderPressed, this, &ControlBar::volumeSliderPressed);
    connect(volumeSlider, &CustomSlider::sliderMoved, this, &ControlBar::volumeSliderMoved);
    connect(volumeSlider, &CustomSlider::sliderReleased, this, &ControlBar::volumeSliderReleased);
    connect(menuButton, &QPushButton::clicked, this, &ControlBar::menuClicked);
    connect(playButton, &QPushButton::clicked, this, &ControlBar::playClicked);
    connect(pauseButton, &QPushButton::clicked, this, &ControlBar::pauseClicked);
    connect(stopButton, &QPushButton::clicked, this, &ControlBar::stopClicked);
    connect(volumeButton, &QPushButton::clicked, this, &ControlBar::volumeClicked);
    connect(muteButton, &QPushButton::clicked, this, &ControlBar::muteClicked);
    connect(fullscreenButton, &QPushButton::clicked, this, &ControlBar::fullscreenClicked);
    connect(speedComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ControlBar::onSpeedChanged);
}

void ControlBar::onSpeedChanged(int index) {
    emit speedChanged(speedTextToFloat(speedComboBox->itemText(index)));
}

float ControlBar::speedTextToFloat(const QString& speedText) const {
    QString tempText = speedText;
    tempText.remove("X");
    bool ok = false;
    float speed = tempText.toFloat(&ok);
    return ok ? speed : 1.0f;
}

QPushButton* ControlBar::createControlButton(const QString& iconPath, const QString& toolTip) {
    QPushButton* button = new QPushButton(this);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(32, 32));
    button->setToolTip(toolTip);
    button->setFixedSize(45, 45);
    button->setStyleSheet(getControlButtonStyle());
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
            color: white;
        }
        QPushButton:hover {
            background: rgba(52, 152, 219, 30);
            border: 2px solid #3498db;
        }
        QPushButton:pressed {
            background: rgba(52, 152, 219, 50);
            border: 2px solid #3498db;
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