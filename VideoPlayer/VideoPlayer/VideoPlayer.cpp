#include "VideoPlayer.h"

VideoPlayer::VideoPlayer(QWidget* parent)
    : QMainWindow(parent)
    , widget(new VideoPlayerWidget(this))
    , filePath("")
    , streamUrl("")
    , state(Idle)
    , buffer(std::make_shared<MediaBuffer>())
    , progressTimer(nullptr)
    , demuxThread(nullptr)
    , videoDecoderThread(nullptr)
    , audioDecoderThread(nullptr)
    , videoPlayThread(nullptr)
    , audioPlayThread(nullptr) {

    setupUi();
    setupConnections();
}

VideoPlayer::~VideoPlayer() {
    cleanup();
}

void VideoPlayer::setupUi() {
    setWindowTitle("Video Player");
    setWindowIcon(QIcon(":/VideoPlayer/icons/player.png"));
    setMinimumSize(960, 540);
    resize(960, 540);
    setupWidget();
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCentralWidget(widget);
}

void VideoPlayer::setupWidget() {
    if (!widget) return;

    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setMouseTracking(true);
    widget->setPlaying(false);
    widget->setTotalTime(0);
    widget->setCurrentTime(0);
    widget->setSpeed(1.0f);
    widget->setVolume(70);
    widget->setMute(false);
    widget->setFullscreen(false);
    widget->setAutoHideEnabled(true);
    widget->setHideDelay(3000);
    widget->setProgress(0);
    widget->setProgressSliderEnabled(true);
    widget->setSpeedComboBoxEnabled(true);
}

void VideoPlayer::setupConnections() {
    if (!widget) return;

    connect(widget, &VideoPlayerWidget::loadLocalVideo, this, &VideoPlayer::onLoadLocalVideo);
    connect(widget, &VideoPlayerWidget::loadNetworkVideo, this, &VideoPlayer::onLoadNetworkVideo);
    connect(widget, &VideoPlayerWidget::playRequested, this, &VideoPlayer::onPlayRequested);
    connect(widget, &VideoPlayerWidget::pauseRequested, this, &VideoPlayer::onPauseRequested);
    connect(widget, &VideoPlayerWidget::seekRequested, this, &VideoPlayer::onSeekRequested);
    connect(widget, &VideoPlayerWidget::speedChanged, this, &VideoPlayer::onSpeedChanged);
    connect(widget, &VideoPlayerWidget::volumeChanged, this, &VideoPlayer::onVolumeChanged);
}

void VideoPlayer::resetWidgetState() {
    if (!widget) return;

    widget->setPlaying(false);
    widget->setTotalTime(0);
    widget->setCurrentTime(0);
    widget->setProgress(0);
    widget->setProgressSliderEnabled(true);
    widget->setSpeedComboBoxEnabled(true);
}

void VideoPlayer::updateWindowTitle(const QString& str) {
    QString title = "Video Player - ";

    media::StreamType type = MediaContext::getInstance().getStreamType();

    switch (type) {
    case media::StreamType::NONE:
        title += QFileInfo(str).fileName();
        break;
    case media::StreamType::VOD:
        title += "Network Stream (VOD)";
        break;
    case media::StreamType::LIVE:
        title += "Network Stream (LIVE)";
        break;
    default:
        title = "Video Player";
        break;
    }

    setWindowTitle(title);
}

void VideoPlayer::setupThreads() {
    cleanupThread(demuxThread);
    cleanupThread(videoDecoderThread);
    cleanupThread(audioDecoderThread);
    cleanupThread(videoPlayThread);
    cleanupThread(audioPlayThread);

    demuxThread = nullptr;
    videoDecoderThread = nullptr;
    audioDecoderThread = nullptr;
    videoPlayThread = nullptr;
    audioPlayThread = nullptr;

    if (progressTimer) {
        progressTimer->stop();
        delete progressTimer;
        progressTimer = nullptr;
    }

    demuxThread = new DemuxThread(this, buffer);
    progressTimer = new QTimer(this);

    float speed = widget ? widget->getCurrentSpeed() : 1.0f;
    int volume = widget ? widget->getCurrentVolume() : 70;

    if (MediaContext::getInstance().hasVideo()) {
        videoDecoderThread = new VideoDecoderThread(this, buffer);
        videoPlayThread = new VideoPlayThread(this, buffer, widget ? widget->getVideoArea() : nullptr);
        if (videoPlayThread) {
            videoPlayThread->setSpeed(speed);
        }
    }

    if (MediaContext::getInstance().hasAudio()) {
        audioDecoderThread = new AudioDecoderThread(this, buffer);
        audioPlayThread = new AudioPlayThread(this, buffer);
        if (audioPlayThread) {
            audioPlayThread->setSpeed(speed);
            audioPlayThread->setVolume(volume);
        }
    }

    setupThreadConnections();

    if (demuxThread) demuxThread->start();
    if (videoDecoderThread) videoDecoderThread->start();
    if (audioDecoderThread) audioDecoderThread->start();

    if (progressTimer) {
        progressTimer->setInterval(500);
    }
}

void VideoPlayer::setupThreadConnections() {
    if (!demuxThread || !progressTimer) return;

    connect(demuxThread, &DemuxThread::demuxError, this, &VideoPlayer::onErrorOccurred);
    connect(progressTimer, &QTimer::timeout, this, &VideoPlayer::onProgressUpdate);

    if (videoDecoderThread) {
        connect(videoDecoderThread, &VideoDecoderThread::videoDecodeError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::flushDecoder, videoDecoderThread, &VideoDecoderThread::onFlushRequest);
    }

    if (videoPlayThread) {
        connect(videoPlayThread, &VideoPlayThread::videoPlayError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::resetClocks, videoPlayThread, &VideoPlayThread::onResetClock);
    }

    if (audioDecoderThread) {
        connect(audioDecoderThread, &AudioDecoderThread::audioDecodeError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::flushDecoder, audioDecoderThread, &AudioDecoderThread::onFlushRequest);
    }

    if (audioPlayThread) {
        connect(audioPlayThread, &AudioPlayThread::audioPlayError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::flushStream, audioPlayThread, &AudioPlayThread::onFlushStream);
    }

    if (audioPlayThread && videoPlayThread) {
        connect(audioPlayThread, &AudioPlayThread::updateAudioClock, videoPlayThread, &VideoPlayThread::onUpdateAudioClock);
    }
}

void VideoPlayer::showErrorMessage(const QString& errorMsg) {
    if (!errorMsg.isEmpty()) {
        QMessageBox::warning(this, "Error", errorMsg);
    }
}

void VideoPlayer::cleanup() {
    if (buffer) buffer->abort();

    if (progressTimer) {
        progressTimer->stop();
        delete progressTimer;
        progressTimer = nullptr;
    }

    cleanupThread(demuxThread);
    cleanupThread(videoDecoderThread);
    cleanupThread(audioDecoderThread);
    cleanupThread(videoPlayThread);
    cleanupThread(audioPlayThread);

    demuxThread = nullptr;
    videoDecoderThread = nullptr;
    audioDecoderThread = nullptr;
    videoPlayThread = nullptr;
    audioPlayThread = nullptr;

    filePath.clear();
    streamUrl.clear();

    MediaContext::getInstance().cleanup();
    if (buffer) {
        buffer->cleanup();
        buffer->resume();
    }

    state = Idle;
}

void VideoPlayer::cleanupThread(QThread* thread) {
    if (!thread) return;

    if (auto* demux = dynamic_cast<DemuxThread*>(thread)) {
        demux->stop();
    }
    else if (auto* videoDec = dynamic_cast<VideoDecoderThread*>(thread)) {
        videoDec->stop();
    }
    else if (auto* audioDec = dynamic_cast<AudioDecoderThread*>(thread)) {
        audioDec->stop();
    }
    else if (auto* videoPlay = dynamic_cast<VideoPlayThread*>(thread)) {
        videoPlay->stop();
    }
    else if (auto* audioPlay = dynamic_cast<AudioPlayThread*>(thread)) {
        audioPlay->stop();
    }
    else {
        thread->quit();
    }

    if (!thread->wait(3000)) {
        thread->requestInterruption();
        if (!thread->wait(1000)) {
            thread->terminate();
            thread->wait(1000);
        }
    }

    delete thread;
}

void VideoPlayer::onLoadLocalVideo(const QString& file) {
    if (file.isEmpty()) {
        showErrorMessage("File path cannot be empty");
        return;
    }

    if (!buffer) {
        showErrorMessage("Media buffer cannot be nullptr (Insufficient memory)");
        return;
    }

    if (state == PlayState::Playing) {
        onPauseRequested();
    }

    if (state != PlayState::Idle) {
        cleanup();
    }

    resetWidgetState();

    filePath = file;
    int ret = MediaContext::getInstance().playLocalFile(filePath);
    if (ret < 0) {
        showErrorMessage(MediaContext::getInstance().getLastError());
        MediaContext::getInstance().cleanup();
        return;
    }

    int64_t totalTime = MediaContext::getInstance().getTotalTime();
    if (totalTime <= 0 || totalTime == AV_NOPTS_VALUE) {
        showErrorMessage("Invalid total time");
        MediaContext::getInstance().cleanup();
        return;
    }

    if (widget) {
        widget->setTotalTime(totalTime);
    }

    updateWindowTitle(filePath);
    setupThreads();

    state = Loaded;
    QTimer::singleShot(50, this, &VideoPlayer::onPlayRequested);
}

void VideoPlayer::onLoadNetworkVideo(const QString& url) {
    if (url.isEmpty()) {
        showErrorMessage("Stream URL cannot be empty");
        return;
    }

    if (!buffer) {
        showErrorMessage("Media buffer cannot be nullptr (Insufficient memory)");
        return;
    }

    if (state == PlayState::Playing) {
        onPauseRequested();
    }

    if (state != PlayState::Idle) {
        cleanup();
    }

    resetWidgetState();

    streamUrl = url;
    int ret = MediaContext::getInstance().playNetworkStream(streamUrl);
    if (ret < 0) {
        showErrorMessage(MediaContext::getInstance().getLastError());
        MediaContext::getInstance().cleanup();
        return;
    }

    bool isLive = MediaContext::getInstance().getStreamType() == media::StreamType::LIVE;
    if (!isLive) {
        int64_t totalTime = MediaContext::getInstance().getTotalTime();
        if (totalTime <= 0 || totalTime == AV_NOPTS_VALUE) {
            showErrorMessage("Invalid total time");
            MediaContext::getInstance().cleanup();
            return;
        }
        if (widget) {
            widget->setTotalTime(totalTime);
        }
    }

    if (isLive && widget) {
        widget->setProgressSliderEnabled(false);
        widget->setSpeedComboBoxEnabled(false);
    }

    updateWindowTitle(streamUrl);
    setupThreads();

    state = Loaded;
    QTimer::singleShot(50, this, &VideoPlayer::onPlayRequested);
}

void VideoPlayer::onPlayRequested() {
    if (state == Error || state == Idle || state == Playing) {
        return;
    }

    if (widget) {
        widget->setPlaying(true);
    }

    if (state == Loaded) {
        if (videoPlayThread) videoPlayThread->start();
        if (audioPlayThread) audioPlayThread->start();
    }
    else {
        if (videoPlayThread) videoPlayThread->resume();
        if (audioPlayThread) audioPlayThread->resume();
    }

    if (progressTimer && !progressTimer->isActive()) {
        progressTimer->start();
    }

    state = Playing;
}

void VideoPlayer::onPauseRequested() {
    if (state == Error || state == Idle || state == Loaded || state == Paused) {
        return;
    }

    if (widget) {
        widget->setPlaying(false);
    }

    if (videoPlayThread) videoPlayThread->pause();
    if (audioPlayThread) audioPlayThread->pause();
    if (progressTimer) progressTimer->stop();

    if (state == Finished) {
        state = Paused;
        if (widget) {
            widget->setCurrentTime(0);
            widget->setProgress(0);
            widget->getVideoArea()->showBackground();
        }

        if (demuxThread) {
            demuxThread->seek(0);
        }
    }
    else {
        state = Paused;
    }
}

void VideoPlayer::onSeekRequested(int position) {
    if (state == Error || state == Idle || state == Loaded || state == Seeking) {
        return;
    }
    
    PlayState oldState = state;
    state = Seeking;

    if (!widget) return;

    int64_t totalTime = widget->getTotalTime();
    if (totalTime <= 0) {
        return;
    }

    int64_t targetTime = totalTime * position / 100;
    widget->setCurrentTime(targetTime);

    if (demuxThread) {
        demuxThread->seek(targetTime);
    }

    if (oldState == Paused || oldState == Finished) {
        onPlayRequested();
    }
    else {
        state = oldState;
    }
}

void VideoPlayer::onSpeedChanged(float speed) {
    if (state == Error || state == Idle || state == Loaded) {
        return;
    }

    if (videoPlayThread) videoPlayThread->setSpeed(speed);
    if (audioPlayThread) audioPlayThread->setSpeed(speed);
}

void VideoPlayer::onVolumeChanged(int volume) {
    if (state == Error || state == Idle || state == Loaded) {
        return;
    }

    if (audioPlayThread) {
        audioPlayThread->setVolume(volume);
    }
}

void VideoPlayer::onProgressUpdate() {
    if (state == Error || state == Idle || state == Loaded || state == Paused || state == Seeking) {
        return;
    }

    if (!videoPlayThread || !widget) return;

    double currentTime = videoPlayThread->getCurrentTime();
    widget->setCurrentTime(static_cast<int>(currentTime));

    int totalTime = widget->getTotalTime();
    if (totalTime > 0) {
        if (static_cast<double>(totalTime) - currentTime < 1.0 && state == Playing) {
            state = Finished;
            QTimer::singleShot(1500, this, &VideoPlayer::onPlaybackFinished);
        }
    }
}

void VideoPlayer::onPlaybackFinished() {
    if (state != Finished) {
        return;
    }

    onPauseRequested();
}

void VideoPlayer::onErrorOccurred(const QString& errorMsg) {
    state = Error;

}