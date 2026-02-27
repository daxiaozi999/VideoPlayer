#include "VideoPlayerUi.h"

VideoPlayerUi::VideoPlayerUi(QWidget* parent)
    : QWidget(parent)
    , mainLayout(nullptr)
    , controlBar(nullptr)
    , videoRenderer(nullptr)
    , fadeAnimation(nullptr)
    , opacityEffect(nullptr)
    , timePreviewLabel(nullptr)
    , volumePreviewLabel(nullptr)
    , autoHideTimer(new QTimer(this))
    , previewHideTimer(new QTimer(this))
    , filePath("")
    , networkUrl("")
    , totalTime(0)
    , currentTime(0)
    , play(false)
    , muted(false)
    , fullscreen(false)
    , speed(1.0f)
    , progress(0)
    , volume(70)
    , autoHideDelay(3000)
    , progressMoved(false)
    , volumeMoved(false)
    , mouseInWindow(false)
    , mouseInControlBar(false)
    , mouseInSpeedComboBox(false) {

    setupUi();
    setupConnections();
}

void VideoPlayerUi::setTotalTime(int64_t totalTime) {
    if (this->totalTime != totalTime) {
        this->totalTime = totalTime;
        updateTimeLabel();
    }
}

void VideoPlayerUi::setCurrentTime(int64_t currentTime) {
    if (this->currentTime != currentTime && !progressMoved) {
        this->currentTime = currentTime;
        updateTimeLabel();
        if (totalTime > 0) {
            int newProgress = qBound(0, static_cast<int>((currentTime * 100 + totalTime / 2) / totalTime), 100);
            controlBar->setProgress(newProgress);
        }
    }
}

void VideoPlayerUi::setPlay(bool play) {
    if (this->play != play) {
        this->play = play;
        updatePlayState();
    }
}

void VideoPlayerUi::setMuted(bool muted) {
    if (this->muted != muted) {
        this->muted = muted;
        controlBar->setVolume(muted ? 0 : volume);
        updateVolumeButton();
    }
}

void VideoPlayerUi::setFullscreen(bool fullscreen) {
    if (this->fullscreen != fullscreen) {
        this->fullscreen = fullscreen;
        controlBar->setFullscreen(this->fullscreen);

        autoHideDelay = fullscreen ? 2000 : 3000;
        autoHideTimer->setInterval(autoHideDelay);
    }
}

void VideoPlayerUi::setSpeed(float speed) {
    if (speed > 0 && !qFuzzyCompare(this->speed, speed)) {
        this->speed = speed;
        controlBar->setSpeed(this->speed);
    }
}

void VideoPlayerUi::setProgress(int progress) {
    progress = qBound(0, progress, 100);
    this->progress = progress;
    controlBar->setProgress(this->progress);
}

void VideoPlayerUi::setVolume(int volume) {
    volume = qBound(0, volume, 100);
    if (this->volume != volume) {
        this->volume = volume;

        if (!muted) {
            controlBar->setVolume(volume);
        }

        if (volume == 0 && !muted) {
            muted = true;
            updateVolumeButton();
        }
        else if (volume > 0 && muted) {
            muted = false;
            updateVolumeButton();
        }
    }
}

void VideoPlayerUi::setProgressSliderEnabled(bool enabled) {
    controlBar->setProgressSliderEnabled(enabled);
}

void VideoPlayerUi::setSpeedComboBoxEnabled(bool enabled) {
    controlBar->setSpeedComboBoxEnabled(enabled);
}

void VideoPlayerUi::resetUiState() {
    setPlay(false);
    setProgress(0);
    setTotalTime(0);
    setCurrentTime(0);
    controlBar->setTimeLabel("00:00", "00:00");
    setProgressSliderEnabled(true);
    setSpeedComboBoxEnabled(true);
    videoRenderer->clearFrame();
}

void VideoPlayerUi::enterEvent(QEvent* event) {
    Q_UNUSED(event);
    mouseInWindow = true;
    updateControlBarVisible();
}

void VideoPlayerUi::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    if (!mouseInSpeedComboBox) {
        mouseInWindow = false;
        updateControlBarVisible();
    }
}

void VideoPlayerUi::mouseMoveEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    mouseInWindow = true;
    showControlBar();
    resetAutoHideTimer();
}

void VideoPlayerUi::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_M) {
        onMenuClicked();
        return;
    }

    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_S) {
        onStopClicked();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Space:
        if (play) {
            emit pauseRequest();
        }
        else {
            emit playRequest();
        }
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal: {
        int newVolume = qMin(100, volume + 5);
        setVolume(newVolume);
        showVolumePreview(newVolume);
        emit volumeChanged(newVolume);
        break;
    }
    case Qt::Key_Minus: {
        int newVolume = qMax(0, volume - 5);
        setVolume(newVolume);
        showVolumePreview(newVolume);
        emit volumeChanged(newVolume);
        break;
    }
    case Qt::Key_Left: {
        if (!controlBar->getProgressSlider()->isEnabled()) {
            break;
        }
        int64_t newTime = qMax(0LL, currentTime - 5);
        int newProgress = 0;
        if (totalTime > 0) {
            newProgress = qBound(0, static_cast<int>((newTime * 100 + totalTime / 2) / totalTime), 100);
        }
        showTimePreview(newProgress);
        emit seekRequest(newProgress);
        break;
    }
    case Qt::Key_Right: {
        if (!controlBar->getProgressSlider()->isEnabled()) {
            break;
        }
        int64_t newTime = qMin(totalTime, currentTime + 5);
        int newProgress = 0;
        if (totalTime > 0) {
            newProgress = qBound(0, static_cast<int>((newTime * 100 + totalTime / 2) / totalTime), 100);
        }
        showTimePreview(newProgress);
        emit seekRequest(newProgress);
        break;
    }
    case Qt::Key_M: {
        setMuted(!muted);
        showVolumePreview(muted ? 0 : volume);
        emit volumeChanged(muted ? 0 : volume);
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
    resetAutoHideTimer();
}

void VideoPlayerUi::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    controlBar->resize(videoRenderer->width(), 120);
    controlBar->move(0, videoRenderer->height() - controlBar->height());
}

void VideoPlayerUi::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && isValidVideoFile(urls.first().toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void VideoPlayerUi::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && isValidVideoFile(urls.first().toLocalFile())) {
        filePath = urls.first().toLocalFile();
        networkUrl.clear();
        emit loadLocalVideo(filePath);
        event->acceptProposedAction();
    }
}

bool VideoPlayerUi::eventFilter(QObject* object, QEvent* event) {
    if (object == controlBar) {
        return handleControlBarEvent(event);
    }

    if (controlBar->getSpeedComboBox()) {
        if (object == controlBar->getSpeedComboBox()) {
            return handleSpeedComboBoxEvent(event);
        }
        QAbstractItemView* view = controlBar->getSpeedComboBox()->view();
        if (view && object == view) {
            return handleSpeedComboBoxViewEvent(event);
        }
    }

    return QWidget::eventFilter(object, event);
}

void VideoPlayerUi::onMenuClicked() {
    MenuDialog dialog(this);
    stopAutoHideTimer();
    showControlBar();

    if (dialog.exec() == QDialog::Accepted) {
        MenuDialog::PlayMode mode = dialog.getPlayMode();
        if (mode == MenuDialog::PlayMode::FILE) {
            filePath = dialog.getFilePath();
            emit loadLocalVideo(filePath);
        }
        else if (mode == MenuDialog::PlayMode::NETWORK) {
            networkUrl = dialog.getNetworkUrl();
            emit loadNetworkVideo(networkUrl);
        }
    }

    if (!mouseInSpeedComboBox && !mouseInControlBar && mouseInWindow) {
        resetAutoHideTimer();
    }
}

void VideoPlayerUi::onPlayClicked() {
    emit playRequest();
}

void VideoPlayerUi::onPauseClicked() {
    emit pauseRequest();
}

void VideoPlayerUi::onStopClicked() {
    emit stopRequest();
}

void VideoPlayerUi::onSpeedChanged(float speed) {
    setSpeed(speed);
    emit speedChanged(this->speed);
}

void VideoPlayerUi::onVolumeClicked() {
    setMuted(true);
    showVolumePreview(0);
    emit volumeChanged(0);
}

void VideoPlayerUi::onMuteClicked() {
    setMuted(false);
    showVolumePreview(volume);
    emit volumeChanged(volume);
}

void VideoPlayerUi::onFullscreenClicked() {
    setFullscreen(!fullscreen);
    QWidget* target = parentWidget() ? parentWidget() : this;

    if (fullscreen) {
        target->showFullScreen();
    }
    else {
        target->showNormal();
    }

    showControlBar();
    resetAutoHideTimer();
}

void VideoPlayerUi::onProgressSliderMoved(int value) {
    progressMoved = true;
    setProgress(value);
    showTimePreview(qBound(0, value, 100));
    emit seekRequest(value);
}

void VideoPlayerUi::onProgressSliderReleased(int value) {
    if (!progressMoved) {
        setProgress(value);
        emit seekRequest(value);
    }

    progressMoved = false;

    QTimer::singleShot(500, this, [this]() {
        hidePreviewLabel();
        if (!mouseInSpeedComboBox && !mouseInControlBar && mouseInWindow) {
            resetAutoHideTimer();
        }
        });
}

void VideoPlayerUi::onVolumeSliderMoved(int value) {
    volumeMoved = true;
    setVolume(value);
    showVolumePreview(volume);
    emit volumeChanged(value);
}

void VideoPlayerUi::onVolumeSliderReleased(int value) {
    if (!volumeMoved) {
        setVolume(value);
        emit volumeChanged(value);
    }

    volumeMoved = false;

    QTimer::singleShot(1000, this, [this]() {
        hidePreviewLabel();
        if (!mouseInSpeedComboBox && !mouseInControlBar && mouseInWindow) {
            resetAutoHideTimer();
        }
        });
}

void VideoPlayerUi::onAutoHideTimeout() {
    if (mouseInControlBar || mouseInSpeedComboBox) {
        autoHideTimer->start();
        return;
    }

    if (mouseInWindow) {
        hideControlBar(300);
    }
    else {
        hideControlsAndCursor();
    }
}

void VideoPlayerUi::onHidePreviewLabel() {
    hidePreviewLabel();
}

void VideoPlayerUi::setupUi() {
    setStyleSheet("VideoPlayerUi { background-color: #2b2b2b; }");
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    videoRenderer = new YUVRenderer(this);
    videoRenderer->setMinimumHeight(400);
    videoRenderer->setMouseTracking(true);
    videoRenderer->installEventFilter(this);

    controlBar = new ControlBar(this);
    controlBar->installEventFilter(this);
    opacityEffect = new QGraphicsOpacityEffect(this);
    controlBar->setGraphicsEffect(opacityEffect);
    opacityEffect->setOpacity(1.0);

    fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    fadeAnimation->setDuration(300);
    fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    mainLayout->addWidget(videoRenderer);
    controlBar->setParent(videoRenderer);
    controlBar->raise();

    if (controlBar->getSpeedComboBox()) {
        controlBar->getSpeedComboBox()->installEventFilter(this);
        QAbstractItemView* view = controlBar->getSpeedComboBox()->view();
        if (view) {
            view->installEventFilter(this);
        }
    }

    setupTimers();
    setupPreviewLabels();
}

void VideoPlayerUi::setupConnections() {
    connect(controlBar, &ControlBar::menuClicked, this, &VideoPlayerUi::onMenuClicked);
    connect(controlBar, &ControlBar::playClicked, this, &VideoPlayerUi::onPlayClicked);
    connect(controlBar, &ControlBar::pauseClicked, this, &VideoPlayerUi::onPauseClicked);
    connect(controlBar, &ControlBar::stopClicked, this, &VideoPlayerUi::onStopClicked);
    connect(controlBar, &ControlBar::speedChanged, this, &VideoPlayerUi::onSpeedChanged);
    connect(controlBar, &ControlBar::volumeClicked, this, &VideoPlayerUi::onVolumeClicked);
    connect(controlBar, &ControlBar::muteClicked, this, &VideoPlayerUi::onMuteClicked);
    connect(controlBar, &ControlBar::fullscreenClicked, this, &VideoPlayerUi::onFullscreenClicked);
    connect(controlBar, &ControlBar::progressSliderMoved, this, &VideoPlayerUi::onProgressSliderMoved);
    connect(controlBar, &ControlBar::progressSliderReleased, this, &VideoPlayerUi::onProgressSliderReleased);
    connect(controlBar, &ControlBar::volumeSliderMoved, this, &VideoPlayerUi::onVolumeSliderMoved);
    connect(controlBar, &ControlBar::volumeSliderReleased, this, &VideoPlayerUi::onVolumeSliderReleased);
}

void VideoPlayerUi::setupTimers() {
    autoHideTimer->setSingleShot(true);
    autoHideTimer->setInterval(autoHideDelay);
    connect(autoHideTimer, &QTimer::timeout, this, &VideoPlayerUi::onAutoHideTimeout);

    previewHideTimer->setSingleShot(true);
    previewHideTimer->setInterval(1500);
    connect(previewHideTimer, &QTimer::timeout, this, &VideoPlayerUi::onHidePreviewLabel);
}

void VideoPlayerUi::setupPreviewLabels() {
    QString previewStyle = R"(
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

    volumePreviewLabel = createPreviewLabel(previewStyle);
    timePreviewLabel = createPreviewLabel(previewStyle);
}

void VideoPlayerUi::updatePlayState() {
    controlBar->setPlayButtonVisible(!play);
    controlBar->setPauseButtonVisible(play);
}

void VideoPlayerUi::updateTimeLabel() {
    if (totalTime > 0) {
        controlBar->setTimeLabel(formatTime(currentTime), formatTime(totalTime));
    }
    else {
        controlBar->setTimeLabel(formatTime(currentTime));
    }
}

void VideoPlayerUi::updateVolumeButton() {
    controlBar->setVolumeButtonVisible(!muted);
    controlBar->setMuteButtonVisible(muted);
}

void VideoPlayerUi::showControlBar() {
    stopAutoHideTimer();
    setCursor(Qt::ArrowCursor);

    if (!controlBar->isVisible()) {
        controlBar->show();
        controlBar->setEnabled(true);
    }

    if (fadeAnimation->state() != QAbstractAnimation::Running) {
        opacityEffect->setOpacity(1.0);
    }
    else {
        fadeAnimation->stop();
        disconnect(fadeAnimation, &QPropertyAnimation::finished, this, nullptr);
        fadeAnimation->setStartValue(opacityEffect->opacity());
        fadeAnimation->setEndValue(1.0);
        fadeAnimation->setDuration(200);
        fadeAnimation->start();
    }
}

void VideoPlayerUi::hideControlBar(int duration) {
    setCursor(Qt::BlankCursor);

    if (controlBar->isVisible()) {
        fadeAnimation->stop();
        disconnect(fadeAnimation, &QPropertyAnimation::finished, this, nullptr);
        fadeAnimation->setStartValue(opacityEffect->opacity());
        fadeAnimation->setEndValue(0.0);
        fadeAnimation->setDuration(duration);

        connect(fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
            if (qFuzzyCompare(fadeAnimation->endValue().toDouble(), 0.0)) {
                controlBar->hide();
                controlBar->setEnabled(false);
            }
            }, Qt::UniqueConnection);

        fadeAnimation->start();
    }
}

void VideoPlayerUi::resetAutoHideTimer() {
    if (!mouseInControlBar && !mouseInSpeedComboBox && mouseInWindow) {
        autoHideTimer->start();
    }
}

void VideoPlayerUi::stopAutoHideTimer() {
    autoHideTimer->stop();
}

void VideoPlayerUi::hideControlsAndCursor() {
    stopAutoHideTimer();
    hideControlBar(150);
}

void VideoPlayerUi::updateControlBarVisible() {
    if (mouseInWindow) {
        showControlBar();
        resetAutoHideTimer();
    }
    else {
        hideControlsAndCursor();
    }
}

bool VideoPlayerUi::handleControlBarEvent(QEvent* event) {
    switch (event->type()) {
    case QEvent::Enter:
        mouseInControlBar = true;
        showControlBar();
        stopAutoHideTimer();
        break;
    case QEvent::Leave:
        mouseInControlBar = false;
        if (mouseInWindow && !mouseInSpeedComboBox) {
            resetAutoHideTimer();
        }
        else if (!mouseInSpeedComboBox) {
            hideControlsAndCursor();
        }
        break;
    default:
        break;
    }

    return false;
}

bool VideoPlayerUi::handleSpeedComboBoxEvent(QEvent* event) {
    if (!controlBar || !controlBar->getSpeedComboBox()) {
        return false;
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::FocusIn:
        mouseInSpeedComboBox = true;
        stopAutoHideTimer();
        showControlBar();
        break;
    case QEvent::FocusOut:
        QTimer::singleShot(200, this, [this]() {
            if (controlBar && controlBar->getSpeedComboBox()) {
                QAbstractItemView* view = controlBar->getSpeedComboBox()->view();
                if (!view || !view->isVisible()) {
                    mouseInSpeedComboBox = false;
                    if (mouseInWindow && !mouseInControlBar) {
                        resetAutoHideTimer();
                    }
                }
            }
            });
        break;
    default:
        break;
    }

    return false;
}

bool VideoPlayerUi::handleSpeedComboBoxViewEvent(QEvent* event) {
    switch (event->type()) {
    case QEvent::Show:
        mouseInSpeedComboBox = true;
        stopAutoHideTimer();
        showControlBar();
        break;
    case QEvent::Hide:
        QTimer::singleShot(200, this, [this]() {
            mouseInSpeedComboBox = false;
            if (mouseInWindow && !mouseInControlBar) {
                resetAutoHideTimer();
            }
            });
        break;
    default:
        break;
    }

    return false;
}

void VideoPlayerUi::hidePreviewLabel() {
    if (volumePreviewLabel) {
        volumePreviewLabel->hide();
    }
    if (timePreviewLabel) {
        timePreviewLabel->hide();
    }
}

void VideoPlayerUi::showTimePreview(int progress) {
    if (totalTime > 0) {
        CustomSlider* progressSlider = controlBar->getProgressSlider();
        if (!progressSlider || !progressSlider->isEnabled()) {
            return;
        }

        int64_t previewTime = (static_cast<int64_t>(progress) * totalTime + 50) / 100;
        QString timeText = QString("%1 / %2").arg(formatTime(previewTime), formatTime(totalTime));
        timePreviewLabel->setText(timeText);
        timePreviewLabel->adjustSize();

        QPoint sliderPos = progressSlider->mapToGlobal(QPoint(0, 0));
        QPoint widgetPos = this->mapFromGlobal(sliderPos);

        int x = widgetPos.x() + (progressSlider->width() - timePreviewLabel->width()) / 2;
        int y = widgetPos.y() - timePreviewLabel->height() - 10;

        x = qBound(10, x, qMax(10, width() - timePreviewLabel->width() - 10));
        y = qBound(10, y, qMax(10, height() - timePreviewLabel->height() - 10));

        timePreviewLabel->move(x, y);
        timePreviewLabel->show();
        timePreviewLabel->raise();
        previewHideTimer->start();
    }
}

void VideoPlayerUi::showVolumePreview(int volume) {
    CustomSlider* volumeSlider = controlBar->getVolumeSlider();
    if (!volumeSlider) {
        return;
    }

    QString text = muted ? "Mute" : QString("%1%").arg(volume);
    volumePreviewLabel->setText(text);
    volumePreviewLabel->adjustSize();

    QPoint sliderPos = volumeSlider->mapToGlobal(QPoint(0, 0));
    QPoint widgetPos = this->mapFromGlobal(sliderPos);

    int x = widgetPos.x() - 5 + (volumeSlider->width() - volumePreviewLabel->width()) / 2;
    int y = height() - 110 - volumePreviewLabel->height();

    x = qBound(10, x, qMax(10, width() - volumePreviewLabel->width() - 10));
    y = qBound(10, y, qMax(10, height() - volumePreviewLabel->height() - 10));

    volumePreviewLabel->move(x, y);
    volumePreviewLabel->show();
    volumePreviewLabel->raise();
    previewHideTimer->start();
}

QLabel* VideoPlayerUi::createPreviewLabel(const QString& styleSheet) {
    QLabel* label = new QLabel(this);
    label->setStyleSheet(styleSheet);
    label->setAlignment(Qt::AlignCenter);
    label->hide();
    return label;
}

bool VideoPlayerUi::isValidVideoFile(const QString& filePath) const {
    if (filePath.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile() || fileInfo.size() < 100) {
        return false;
    }

    static const QStringList extensions = {
        "mp4", "m4v", "mov", "avi", "mkv", "wmv",
        "flv", "3gp", "ts", "webm", "mpg", "mpeg",
        "f4v", "rmvb", "rm", "asf", "divx", "xvid"
    };

    QString suffix = fileInfo.suffix().toLower();
    if (extensions.contains(suffix)) {
        return true;
    }

    QMimeDatabase mimeDatabase;
    QMimeType mimeType = mimeDatabase.mimeTypeForFile(fileInfo);
    return mimeType.name().startsWith("video/") ||
          (mimeType.name().startsWith("application/") &&
          (mimeType.name().contains("mp4") || mimeType.name().contains("mpeg")));
}

QString VideoPlayerUi::formatTime(int64_t seconds) const {
    if (seconds < 0) {
        seconds = 0;
    }

    int64_t hours = seconds / 3600;
    int64_t minutes = (seconds % 3600) / 60;
    int64_t secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }

    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}