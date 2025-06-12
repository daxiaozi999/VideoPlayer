#include "VideoPlayerWidget.h"

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QWidget(parent)
    , mainLayout(nullptr)
    , videoArea(nullptr)
    , controlBar(nullptr)
    , opacityEffect(nullptr)
    , fadeAnimation(nullptr)
    , volumePreviewLabel(nullptr)
    , timePreviewLabel(nullptr)
    , progressPreviewLabel(nullptr)
    , hideTimer(new QTimer(this))
    , cursorHideTimer(new QTimer(this))
    , previewHideTimer(new QTimer(this))
    , currentFilePath("")
    , currentUrl("")
    , playing(false)
    , totalTime(0)
    , currentTime(0)
    , speed(1.0f)
    , currentProgress(0)
    , currentVolume(50)
    , volumeBeforeMute(50)
    , muted(false)
    , fullscreen(false)
    , hideDelay(3000)
    , cursorHideDelay(5000)
    , autoHideEnabled(true)
    , mouseInWidget(false)
    , isMouseOverControlBar(false)
    , isMouseOverSpeedComboBox(false)
    , controlBarVisible(true) {

    setupUi();
    setupConnections();
    setupTimers();
    setupPreviewLabels();
    hideControlBarDelayed();
}

void VideoPlayerWidget::setPlaying(bool isPlaying) {
    if (playing == isPlaying) return;
    playing = isPlaying;
    updatePlayPauseButtons();
    updateControlBarVisibility();
}

void VideoPlayerWidget::setTotalTime(int64_t totalSeconds) {
    if (totalTime != totalSeconds) {
        totalTime = totalSeconds;
        updateTimeLabel();
    }
}

void VideoPlayerWidget::setCurrentTime(int64_t currentSeconds) {
    if (currentTime != currentSeconds) {
        currentTime = currentSeconds;
        updateTimeLabel();
        if (totalTime > 0) {
            int progress = static_cast<int>((currentSeconds * 100) / totalTime);
            currentProgress = qBound(0, progress, 100);
            if (controlBar) {
                controlBar->setProgress(currentProgress);
            }
        }
    }
}

void VideoPlayerWidget::setSpeed(float newSpeed) {
    if (newSpeed <= 0 || qFuzzyCompare(speed, newSpeed)) return;

    speed = newSpeed;
    if (controlBar) {
        controlBar->setPlaybackSpeed(newSpeed);
    }
}

void VideoPlayerWidget::setVolume(int volume) {
    volume = qBound(0, volume, 100);
    if (currentVolume == volume) return;

    if (!muted && volume > 0) {
        volumeBeforeMute = volume;
    }
    currentVolume = volume;

    if (controlBar) {
        controlBar->setVolume(volume);
    }

    bool wasMuted = muted;
    muted = (volume == 0);
    if (wasMuted != muted) {
        updateVolumeButtons();
    }
}

void VideoPlayerWidget::setMute(bool isMuted) {
    if (muted == isMuted) return;

    muted = isMuted;
    if (muted) {
        if (currentVolume > 0) {
            volumeBeforeMute = currentVolume;
        }
        currentVolume = 0;
    }
    else {
        currentVolume = volumeBeforeMute > 0 ? volumeBeforeMute : 50;
    }

    if (controlBar) {
        controlBar->setVolume(currentVolume);
    }
    updateVolumeButtons();
}

void VideoPlayerWidget::setFullscreen(bool isFullscreen) {
    if (fullscreen == isFullscreen) return;

    fullscreen = isFullscreen;
    if (controlBar) {
        controlBar->setFullscreen(isFullscreen);
    }

    hideDelay = fullscreen ? 2000 : 3000;
    cursorHideDelay = fullscreen ? 3000 : 5000;

    if (hideTimer) {
        hideTimer->setInterval(hideDelay);
    }
    if (cursorHideTimer) {
        cursorHideTimer->setInterval(cursorHideDelay);
    }
}

void VideoPlayerWidget::setAutoHideEnabled(bool enabled) {
    autoHideEnabled = enabled;
    if (!enabled) {
        if (hideTimer) hideTimer->stop();
        if (cursorHideTimer) cursorHideTimer->stop();
        showControlBar();
        setCursor(Qt::ArrowCursor);
    }
    else {
        updateControlBarVisibility();
    }
}

void VideoPlayerWidget::setHideDelay(int milliseconds) {
    hideDelay = qMax(1000, milliseconds);
    if (hideTimer) {
        hideTimer->setInterval(hideDelay);
    }
}

void VideoPlayerWidget::setProgress(int progress) {
    progress = qBound(0, progress, 100);
    if (currentProgress == progress) return;

    currentProgress = progress;
    if (controlBar) {
        controlBar->setProgress(currentProgress);
    }
}

void VideoPlayerWidget::setProgressSliderEnabled(bool enable) {
    if (controlBar) {
        controlBar->setProgressSliderEnabled(enable);
    }
}

void VideoPlayerWidget::setSpeedComboBoxEnabled(bool enable) {
    if (controlBar) {
        controlBar->setSpeedComboBoxEnabled(enable);
    }
}

void VideoPlayerWidget::enterEvent(QEvent* event) {
    Q_UNUSED(event);
    updateMouseState(true);
}

void VideoPlayerWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    if (!isMouseOverSpeedComboBox) {
        updateMouseState(false);
    }
}

void VideoPlayerWidget::mouseMoveEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    updateMouseState(true);
    showControlBar();
    hideControlBarDelayed();
    resetCursorTimer();
}

void VideoPlayerWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_S:
        onSettingsClicked();
        break;
    case Qt::Key_Space:
        if (playing) {
            emit pauseRequested();
        }
        else {
            emit playRequested();
        }
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal: {
        int newVolume = qMin(100, currentVolume + 5);
        setVolume(newVolume);
        showVolumePreview(newVolume);
        emit volumeChanged(newVolume);
        break;
    }
    case Qt::Key_Minus: {
        int newVolume = qMax(0, currentVolume - 5);
        setVolume(newVolume);
        showVolumePreview(newVolume);
        emit volumeChanged(newVolume);
        break;
    }
    case Qt::Key_Left: {
        int newProgress = qMax(0, currentProgress - 5);
        setProgress(newProgress);
        emit seekRequested(newProgress);
        break;
    }
    case Qt::Key_Right: {
        int newProgress = qMin(100, currentProgress + 5);
        setProgress(newProgress);
        emit seekRequested(newProgress);
        break;
    }
    case Qt::Key_M: {
        bool newMuted = !muted;
        setMute(newMuted);
        showVolumePreview(currentVolume);
        emit volumeChanged(currentVolume);
        break;
    }
    case Qt::Key_F:
        onFullscreenClicked();
        break;
    case Qt::Key_Escape:
        if (fullscreen) {
            onFullscreenClicked();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
    showControlBar();
    hideControlBarDelayed();
    resetCursorTimer();
}

void VideoPlayerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (controlBar && videoArea) {
        controlBar->resize(videoArea->width(), 120);
        controlBar->move(0, videoArea->height() - controlBar->height());
    }
}

void VideoPlayerWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && isValidVideoFile(urls.first().toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void VideoPlayerWidget::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && isValidVideoFile(urls.first().toLocalFile())) {
        loadDroppedVideo(urls.first().toLocalFile());
        event->acceptProposedAction();
    }
}

bool VideoPlayerWidget::eventFilter(QObject* object, QEvent* event) {
    if (object == controlBar) return handleControlBarEvent(event);
    if (object == videoArea) return handleVideoAreaEvent(event);

    if (controlBar && controlBar->getSpeedComboBox()) {
        if (object == controlBar->getSpeedComboBox()) {
            return handleSpeedComboBoxEvent(event);
        }
        if (object == controlBar->getSpeedComboBox()->view()) {
            return handleSpeedComboBoxViewEvent(event);
        }
    }
    return QWidget::eventFilter(object, event);
}

void VideoPlayerWidget::onSettingsClicked() {
    SettingsDialog dialog(this);
    if (hideTimer) hideTimer->stop();
    showControlBar();

    if (dialog.exec() == QDialog::Accepted) {
        PlayMode mode = dialog.getPlayMode();
        if (mode == PlayMode::LOCAL_MODE) {
            currentFilePath = dialog.getFilePath();
            emit loadLocalVideo(currentFilePath);
        }
        else if (mode == PlayMode::NETWORK_MODE) {
            currentUrl = dialog.getNetworkUrl();
            emit loadNetworkVideo(currentUrl);
        }
    }

    if (autoHideEnabled) {
        hideControlBarDelayed();
    }
}

void VideoPlayerWidget::onPlayClicked() {
    emit playRequested();
}

void VideoPlayerWidget::onPauseClicked() {
    emit pauseRequested();
}

void VideoPlayerWidget::onVolumeButtonClicked() {
    setMute(true);
    emit volumeChanged(currentVolume);
}

void VideoPlayerWidget::onMuteButtonClicked() {
    setMute(false);
    showVolumePreview(currentVolume);
    emit volumeChanged(currentVolume);
}

void VideoPlayerWidget::onFullscreenClicked() {
    bool newFullscreen = !fullscreen;
    QWidget* target = parentWidget() ? parentWidget() : this;

    if (newFullscreen) {
        target->showFullScreen();
    }
    else {
        target->showNormal();
    }

    setFullscreen(newFullscreen);
    showControlBar();
    hideControlBarDelayed();
}

void VideoPlayerWidget::onVolumeSliderClicked(int value) {
    if (hideTimer) hideTimer->stop();
    if (cursorHideTimer) cursorHideTimer->stop();
    setCursor(Qt::ArrowCursor);
    setVolume(value);
    emit volumeChanged(value);
}

void VideoPlayerWidget::onVolumeSliderMoved(int value) {
    showVolumePreview(qBound(0, value, 100));
}

void VideoPlayerWidget::onVolumeSliderReleased(int value) {
    setVolume(value);
    emit volumeChanged(value);
    QTimer::singleShot(1000, this, [this]() {
        hideAllPreviewLabels();
        if (autoHideEnabled) {
            hideControlBarDelayed();
        }
        });
}

void VideoPlayerWidget::onProgressSliderClicked(int value) {
    if (hideTimer) hideTimer->stop();
    if (cursorHideTimer) cursorHideTimer->stop();
    setCursor(Qt::ArrowCursor);
    setProgress(value);
    emit seekRequested(value);
}

void VideoPlayerWidget::onProgressSliderMoved(int value) {
    showTimePreview(qBound(0, value, 100));
}

void VideoPlayerWidget::onProgressSliderReleased(int value) {
    setProgress(value);
    emit seekRequested(value);
    QTimer::singleShot(500, this, [this]() {
        hideAllPreviewLabels();
        if (autoHideEnabled) {
            hideControlBarDelayed();
        }
        });
}

void VideoPlayerWidget::onSpeedChanged(float newSpeed) {
    setSpeed(newSpeed);
    emit speedChanged(newSpeed);
}

void VideoPlayerWidget::onHideControlBar() {
    if (!autoHideEnabled) return;

    if (isControlBarInteractionActive() || isMouseOverControlBar) {
        if (hideTimer) hideTimer->start();
        return;
    }

    if (mouseInWidget) {
        hideControlBar();
    }
    else {
        hideControlBarImmediately();
    }
}

void VideoPlayerWidget::onHideCursor() {
    updateCursorVisibility();
}

void VideoPlayerWidget::onHidePreviewLabels() {
    hideAllPreviewLabels();
}

void VideoPlayerWidget::setupUi() {
    setStyleSheet("VideoPlayerWidget { background-color: #2b2b2b; }");
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    videoArea = new YUVRenderer(this);
    videoArea->setMinimumHeight(400);
    videoArea->setMouseTracking(true);

    controlBar = new ControlBar(this);
    opacityEffect = new QGraphicsOpacityEffect(this);
    controlBar->setGraphicsEffect(opacityEffect);
    opacityEffect->setOpacity(1.0);

    fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    fadeAnimation->setDuration(300);
    fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    mainLayout->addWidget(videoArea);
    controlBar->setParent(videoArea);
    controlBar->raise();
}

void VideoPlayerWidget::setupConnections() {
    if (!controlBar) return;

    connect(controlBar, &ControlBar::settingsClicked, this, &VideoPlayerWidget::onSettingsClicked);
    connect(controlBar, &ControlBar::playClicked, this, &VideoPlayerWidget::onPlayClicked);
    connect(controlBar, &ControlBar::pauseClicked, this, &VideoPlayerWidget::onPauseClicked);
    connect(controlBar, &ControlBar::volumeButtonClicked, this, &VideoPlayerWidget::onVolumeButtonClicked);
    connect(controlBar, &ControlBar::muteButtonClicked, this, &VideoPlayerWidget::onMuteButtonClicked);
    connect(controlBar, &ControlBar::fullscreenClicked, this, &VideoPlayerWidget::onFullscreenClicked);
    connect(controlBar, &ControlBar::volumeSliderClicked, this, &VideoPlayerWidget::onVolumeSliderClicked);
    connect(controlBar, &ControlBar::volumeSliderMoved, this, &VideoPlayerWidget::onVolumeSliderMoved);
    connect(controlBar, &ControlBar::volumeSliderReleased, this, &VideoPlayerWidget::onVolumeSliderReleased);
    connect(controlBar, &ControlBar::progressSliderClicked, this, &VideoPlayerWidget::onProgressSliderClicked);
    connect(controlBar, &ControlBar::progressSliderMoved, this, &VideoPlayerWidget::onProgressSliderMoved);
    connect(controlBar, &ControlBar::progressSliderReleased, this, &VideoPlayerWidget::onProgressSliderReleased);
    connect(controlBar, &ControlBar::speedChanged, this, &VideoPlayerWidget::onSpeedChanged);

    controlBar->installEventFilter(this);
    if (videoArea) {
        videoArea->installEventFilter(this);
    }

    if (controlBar->getSpeedComboBox()) {
        controlBar->getSpeedComboBox()->installEventFilter(this);
        if (controlBar->getSpeedComboBox()->view()) {
            controlBar->getSpeedComboBox()->view()->installEventFilter(this);
        }
    }
}

void VideoPlayerWidget::setupTimers() {
    if (hideTimer) {
        hideTimer->setSingleShot(true);
        hideTimer->setInterval(hideDelay);
        connect(hideTimer, &QTimer::timeout, this, &VideoPlayerWidget::onHideControlBar);
    }

    if (cursorHideTimer) {
        cursorHideTimer->setSingleShot(true);
        cursorHideTimer->setInterval(cursorHideDelay);
        connect(cursorHideTimer, &QTimer::timeout, this, &VideoPlayerWidget::onHideCursor);
    }

    if (previewHideTimer) {
        previewHideTimer->setSingleShot(true);
        previewHideTimer->setInterval(1500);
        connect(previewHideTimer, &QTimer::timeout, this, &VideoPlayerWidget::onHidePreviewLabels);
    }
}

void VideoPlayerWidget::setupPreviewLabels() {
    QString unifiedPreviewStyle = R"(
        QLabel {
            background-color: rgba(0, 0, 0, 180);
            color: #ffffff;
            border: 2px solid #3498db;
            border-radius: 10px;
            font-size: 18px;
            font-weight: bold;
            padding: 10px 14px;
        }
    )";

    volumePreviewLabel = createPreviewLabel(unifiedPreviewStyle);
    timePreviewLabel = createPreviewLabel(unifiedPreviewStyle);
    progressPreviewLabel = createPreviewLabel(unifiedPreviewStyle);
}

QLabel* VideoPlayerWidget::createPreviewLabel(const QString& styleSheet) {
    QLabel* label = new QLabel(this);
    label->setStyleSheet(styleSheet);
    label->setAlignment(Qt::AlignCenter);
    label->hide();
    return label;
}

void VideoPlayerWidget::showControlBar() {
    if (!controlBar) return;

    if (hideTimer) hideTimer->stop();

    if (!controlBarVisible) {
        controlBar->show();
        controlBar->setEnabled(true);
        controlBarVisible = true;
    }

    if (fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
        opacityEffect->setOpacity(1.0);
    }
    else if (fadeAnimation) {
        cleanupFadeAnimation();
        fadeAnimation->setStartValue(opacityEffect->opacity());
        fadeAnimation->setEndValue(1.0);
        fadeAnimation->setDuration(200);
        fadeAnimation->start();
    }
}

void VideoPlayerWidget::hideControlBar() {
    hideControlBarWithDuration(300);
}

void VideoPlayerWidget::hideControlBarDelayed() {
    if (autoHideEnabled && !isMouseOverControlBar && !isMouseOverSpeedComboBox && mouseInWidget) {
        if (hideTimer) hideTimer->start();
    }
}

void VideoPlayerWidget::hideControlBarImmediately() {
    if (hideTimer) hideTimer->stop();
    hideControlBarWithDuration(150);
}

void VideoPlayerWidget::hideControlBarWithDuration(int duration) {
    if (!controlBar || !controlBarVisible) return;

    if (fadeAnimation) {
        cleanupFadeAnimation();
        fadeAnimation->setStartValue(opacityEffect->opacity());
        fadeAnimation->setEndValue(0.0);
        fadeAnimation->setDuration(duration);

        connect(fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
            if (fadeAnimation && qFuzzyCompare(fadeAnimation->endValue().toDouble(), 0.0)) {
                if (controlBar) {
                    controlBar->hide();
                    controlBar->setEnabled(false);
                }
                controlBarVisible = false;
            }
            }, Qt::UniqueConnection);

        fadeAnimation->start();
    }
}

void VideoPlayerWidget::cleanupFadeAnimation() {
    if (fadeAnimation) {
        fadeAnimation->stop();
        disconnect(fadeAnimation, &QPropertyAnimation::finished, this, nullptr);
    }
}

void VideoPlayerWidget::updateControlBarVisibility() {
    if (mouseInWidget) {
        showControlBar();
        hideControlBarDelayed();
        resetCursorTimer();
    }
    else if (autoHideEnabled) {
        hideControlBarImmediately();
        setCursor(Qt::BlankCursor);
    }
}

bool VideoPlayerWidget::isControlBarInteractionActive() const {
    if (!controlBar) return false;
    if (isMouseOverSpeedComboBox) return true;

    QComboBox* speedComboBox = controlBar->getSpeedComboBox();
    return speedComboBox && (speedComboBox->hasFocus() || speedComboBox->view()->isVisible());
}

void VideoPlayerWidget::updateMouseState(bool inWidget) {
    mouseInWidget = inWidget;
    updateControlBarVisibility();
}

void VideoPlayerWidget::resetCursorTimer() {
    if (cursorHideTimer) {
        cursorHideTimer->stop();
        if (mouseInWidget && !isMouseOverControlBar && !isMouseOverSpeedComboBox) {
            cursorHideTimer->start();
        }
    }
}

void VideoPlayerWidget::updateCursorVisibility() {
    if (!isMouseOverControlBar && !isMouseOverSpeedComboBox && mouseInWidget) {
        setCursor(Qt::BlankCursor);
    }
}

bool VideoPlayerWidget::handleControlBarEvent(QEvent* event) {
    switch (event->type()) {
    case QEvent::Enter:
        isMouseOverControlBar = true;
        showControlBar();
        setCursor(Qt::ArrowCursor);
        if (cursorHideTimer) cursorHideTimer->stop();
        if (hideTimer) hideTimer->stop();
        break;
    case QEvent::Leave:
        isMouseOverControlBar = false;
        if (mouseInWidget && !isMouseOverSpeedComboBox) {
            hideControlBarDelayed();
            resetCursorTimer();
        }
        else if (!isMouseOverSpeedComboBox) {
            hideControlBarImmediately();
            setCursor(Qt::BlankCursor);
        }
        break;
    default:
        break;
    }
    return false;
}

bool VideoPlayerWidget::handleVideoAreaEvent(QEvent* event) {
    switch (event->type()) {
    case QEvent::MouseMove:
        updateMouseState(true);
        showControlBar();
        hideControlBarDelayed();
        resetCursorTimer();
        break;
    case QEvent::Enter:
        updateMouseState(true);
        setCursor(Qt::ArrowCursor);
        if (cursorHideTimer) cursorHideTimer->stop();
        break;
    case QEvent::Leave:
        if (!isMouseOverSpeedComboBox) {
            updateMouseState(false);
        }
        break;
    default:
        break;
    }
    return false;
}

bool VideoPlayerWidget::handleSpeedComboBoxEvent(QEvent* event) {
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::FocusIn:
        isMouseOverSpeedComboBox = true;
        if (hideTimer) hideTimer->stop();
        showControlBar();
        break;
    case QEvent::FocusOut:
        QTimer::singleShot(200, this, [this]() {
            if (controlBar && controlBar->getSpeedComboBox() &&
                !controlBar->getSpeedComboBox()->view()->isVisible()) {
                isMouseOverSpeedComboBox = false;
                if (mouseInWidget && !isMouseOverControlBar) {
                    hideControlBarDelayed();
                }
            }
            });
        break;
    default:
        break;
    }
    return false;
}

bool VideoPlayerWidget::handleSpeedComboBoxViewEvent(QEvent* event) {
    switch (event->type()) {
    case QEvent::Show:
        isMouseOverSpeedComboBox = true;
        if (hideTimer) hideTimer->stop();
        showControlBar();
        break;
    case QEvent::Hide:
        QTimer::singleShot(200, this, [this]() {
            isMouseOverSpeedComboBox = false;
            if (mouseInWidget && !isMouseOverControlBar) {
                hideControlBarDelayed();
            }
            });
        break;
    default:
        break;
    }
    return false;
}

bool VideoPlayerWidget::isValidVideoFile(const QString& filePath) const {
    if (filePath.isEmpty()) return false;

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) return false;

    if (fileInfo.size() < 100) return false;

    static const QStringList videoExtensions = {
        "mp4", "m4v", "mov", "avi", "mkv", "wmv", "flv", "3gp", "ts", "webm", "mpg", "mpeg",
        "f4v", "rmvb", "rm", "asf", "divx", "xvid"
    };

    QString suffix = fileInfo.suffix().toLower();
    if (videoExtensions.contains(suffix)) return true;

    QMimeType mimeType = mimeDatabase.mimeTypeForFile(fileInfo);
    return mimeType.name().startsWith("video/") ||
        (mimeType.name().startsWith("application/") &&
            (mimeType.name().contains("mp4") || mimeType.name().contains("mpeg")));
}

void VideoPlayerWidget::loadDroppedVideo(const QString& filePath) {
    currentFilePath = filePath;
    currentUrl.clear();
    emit loadLocalVideo(currentFilePath);
}

void VideoPlayerWidget::updatePlayPauseButtons() {
    if (controlBar) {
        controlBar->setPlayButtonVisible(!playing);
        controlBar->setPauseButtonVisible(playing);
    }
}

void VideoPlayerWidget::updateVolumeButtons() {
    if (controlBar) {
        bool shouldShowMute = (currentVolume == 0 || muted);
        controlBar->setVolumeButtonVisible(!shouldShowMute);
        controlBar->setMuteButtonVisible(shouldShowMute);
    }
}

void VideoPlayerWidget::updateTimeLabel() {
    if (!controlBar) return;

    if (totalTime > 0) {
        QString currentTimeText = formatTime(currentTime);
        QString totalTimeText = formatTime(totalTime);
        controlBar->setTimeLabel(currentTimeText, totalTimeText);
    }
    else {
        controlBar->setTimeLabel(formatTime(currentTime));
    }
}

void VideoPlayerWidget::showVolumePreview(int volume) {
    if (!volumePreviewLabel || !controlBar) return;

    QString text = muted ? "Mute" : QString("%1%").arg(volume);
    volumePreviewLabel->setText(text);
    volumePreviewLabel->adjustSize();

    positionVolumePreviewLabel(volumePreviewLabel);
    volumePreviewLabel->show();
    volumePreviewLabel->raise();

    if (previewHideTimer) {
        previewHideTimer->start();
    }
}

void VideoPlayerWidget::showTimePreview(int progress) {
    if (!timePreviewLabel || !controlBar || totalTime <= 0) return;

    int64_t previewTime = calculatePreviewTime(progress);
    QString timeText = QString("%1 / %2").arg(formatTime(previewTime), formatTime(totalTime));
    timePreviewLabel->setText(timeText);
    timePreviewLabel->adjustSize();

    positionTimePreviewLabel(timePreviewLabel);
    timePreviewLabel->show();
    timePreviewLabel->raise();

    if (previewHideTimer) {
        previewHideTimer->start();
    }
}

void VideoPlayerWidget::positionVolumePreviewLabel(QLabel* label) {
    if (!label || !controlBar) return;

    CustomSlider* volumeSlider = controlBar->getVolumeSlider();
    if (!volumeSlider) return;

    QPoint sliderPos = volumeSlider->mapToGlobal(QPoint(0, 0));
    QPoint widgetPos = this->mapFromGlobal(sliderPos);

    int x = widgetPos.x() - 5 + (volumeSlider->width() - label->width()) / 2;
    int y = height() - 110 - label->height();

    x = qBound(10, x, width() - label->width() - 10);
    y = qBound(10, y, height() - label->height() - 10);

    label->move(x, y);
}

void VideoPlayerWidget::positionTimePreviewLabel(QLabel* label) {
    if (!label || !controlBar) return;

    CustomSlider* progressSlider = controlBar->getProgressSlider();
    if (!progressSlider) return;

    QPoint sliderPos = progressSlider->mapToGlobal(QPoint(0, 0));
    QPoint widgetPos = this->mapFromGlobal(sliderPos);

    int x = widgetPos.x() + (progressSlider->width() - label->width()) / 2;
    int y = widgetPos.y() - label->height() - 10;

    x = qBound(10, x, width() - label->width() - 10);
    y = qBound(10, y, height() - label->height() - 10);

    label->move(x, y);
}

void VideoPlayerWidget::hideAllPreviewLabels() {
    if (volumePreviewLabel) volumePreviewLabel->hide();
    if (timePreviewLabel) timePreviewLabel->hide();
    if (progressPreviewLabel) progressPreviewLabel->hide();
}

QString VideoPlayerWidget::formatTime(int64_t seconds) const {
    if (seconds < 0) seconds = 0;

    int64_t hours = seconds / 3600;
    int64_t minutes = (seconds % 3600) / 60;
    int64_t secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

int64_t VideoPlayerWidget::calculatePreviewTime(int progress) const {
    if (totalTime <= 0 || progress < 0 || progress > 100) return 0;
    return (progress * totalTime) / 100;
}