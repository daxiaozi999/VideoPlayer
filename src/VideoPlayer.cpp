#include "VideoPlayer.h"

VideoPlayer::VideoPlayer(QWidget* parent)
    : QMainWindow(parent)
    , ui(new VideoPlayerUi(this))
    , state(Idle)
    , filePath("")
    , networkUrl("")
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
    setMinimumSize(1280, 720);
    resize(1280, 720);
    setFocusPolicy(Qt::StrongFocus);
    setWindowState(Qt::WindowActive);
    setCentralWidget(ui);
    ui->resetUiState();
}

void VideoPlayer::setupConnections() {
    connect(ui, &VideoPlayerUi::loadLocalVideo, this, &VideoPlayer::onLoadLocalVideo);
    connect(ui, &VideoPlayerUi::loadNetworkVideo, this, &VideoPlayer::onLoadNetworkVideo);
    connect(ui, &VideoPlayerUi::playRequest, this, &VideoPlayer::onPlayRequest);
    connect(ui, &VideoPlayerUi::pauseRequest, this, &VideoPlayer::onPauseRequest);
    connect(ui, &VideoPlayerUi::stopRequest, this, &VideoPlayer::onStopRequest);
    connect(ui, &VideoPlayerUi::seekRequest, this, &VideoPlayer::onSeekRequest);
    connect(ui, &VideoPlayerUi::speedChanged, this, &VideoPlayer::onSpeedChanged);
    connect(ui, &VideoPlayerUi::volumeChanged, this, &VideoPlayer::onVolumeChanged);
}

void VideoPlayer::setupThreads() {
    demuxThread = new DemuxThread(this, buffer);
    progressTimer = new QTimer(this);
    progressTimer->setInterval(500);

    int volume = ui->getVolume();
    float speed = MCTX()->mediaInput()->duration() <= 0 ? 1.0f : ui->getSpeed();

    if (MCTX()->mediaInput()->hasVideoStream()) {
        videoDecoderThread = new VideoDecodeThread(this, buffer);
        videoPlayThread = new VideoPlayThread(this, ui->getVideoRenderer(), buffer);
        videoPlayThread->setSpeed(speed);
    }

    if (MCTX()->mediaInput()->hasAudioStream()) {
        audioDecoderThread = new AudioDecodeThread(this, buffer);
        audioPlayThread = new AudioPlayThread(this, buffer);
        audioPlayThread->setVolume(volume);
        audioPlayThread->setSpeed(speed);
    }

    setupThreadConnections();
}

void VideoPlayer::setupThreadConnections() {
    connect(demuxThread, &DemuxThread::demuxError, this, &VideoPlayer::onErrorOccurred);
    connect(progressTimer, &QTimer::timeout, this, &VideoPlayer::onUpdateProgress);

    if (videoDecoderThread) {
        connect(videoDecoderThread, &VideoDecodeThread::videoDecodeError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::flushRequest, videoDecoderThread, &VideoDecodeThread::onFlushRequest);
    }

    if (videoPlayThread) {
        connect(videoPlayThread, &VideoPlayThread::videoPlayError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::flushRequest, videoPlayThread, &VideoPlayThread::onFlushRequest);
    }

    if (audioDecoderThread) {
        connect(audioDecoderThread, &AudioDecodeThread::audioDecodeError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::flushRequest, audioDecoderThread, &AudioDecodeThread::onFlushRequest);
    }

    if (audioPlayThread) {
        connect(audioPlayThread, &AudioPlayThread::audioPlayError, this, &VideoPlayer::onErrorOccurred);
        connect(demuxThread, &DemuxThread::flushRequest, audioPlayThread, &AudioPlayThread::onFlushStream);
    }

    if (audioPlayThread && videoPlayThread) {
        connect(audioPlayThread, &AudioPlayThread::updateAudioClock, videoPlayThread, &VideoPlayThread::onUpdateAudioClock);
    }
}

void VideoPlayer::handleError(const QString& error) {
    showErrorMessage(error);
    cleanup();
    setWindowTitle("Video Player");
    ui->resetUiState();
    state = Idle;
}

void VideoPlayer::showErrorMessage(const QString& error) {
    if (!error.isEmpty()) {
        QMessageBox::warning(this, "Error", error);
    }
}

void VideoPlayer::cleanup() {
    buffer->lock();

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

    MCTX()->reset();

    buffer->clear();
    buffer->unlock();
}

void VideoPlayer::cleanupThread(QThread* thread) {
    if (!thread) {
        return;
    }

    if (auto* demux = dynamic_cast<DemuxThread*>(thread)) {
        demux->stop();
    }
    else if (auto* videoDec = dynamic_cast<VideoDecodeThread*>(thread)) {
        videoDec->stop();
    }
    else if (auto* audioDec = dynamic_cast<AudioDecodeThread*>(thread)) {
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

void VideoPlayer::onLoadLocalVideo(const QString& filePath) {
    if (state == Loading) {
        return;
    }

    state = Loading;

    if (filePath.isEmpty()) {
        handleError("File path cannot be empty");
        return;
    }

    if (!buffer) {
        handleError("Media buffer cannot be nullptr");
        return;
    }

    cleanup();
    ui->resetUiState();

    int ret = MCTX()->playFileStream(filePath.toStdString());
    if (ret < 0) {
        handleError(QString(MCTX()->error().c_str()));
        return;
    }

    int64_t totalTime = MCTX()->mediaInput()->duration();
    if (totalTime <= 0 || totalTime == AV_NOPTS_VALUE) {
        handleError("Invalid total time");
        return;
    }

    ui->setTotalTime(totalTime);

    this->filePath = filePath;
    setWindowTitle(QString("Video Player - %1").arg(QFileInfo(filePath).fileName()));
    setupThreads();

    state = Loaded;
    QTimer::singleShot(50, this, [this]() {
        if (state == Loaded) {
            onPlayRequest();
        }
        });
}

void VideoPlayer::onLoadNetworkVideo(const QString& networkUrl) {
    if (state == Loading) {
        return;
    }

    state = Loading;

    if (networkUrl.isEmpty()) {
        handleError("Network URL cannot be empty");
        return;
    }

    if (!buffer) {
        handleError("Media buffer cannot be nullptr");
        return;
    }

    cleanup();
    ui->resetUiState();

    int ret = MCTX()->playNetworkStream(networkUrl.toStdString());
    if (ret < 0) {
        handleError(QString(MCTX()->error().c_str()));
        return;
    }

    if (MCTX()->mediaInput()->duration() > 0) {
        int64_t totalTime = MCTX()->mediaInput()->duration();
        if (totalTime <= 0 || totalTime == AV_NOPTS_VALUE) {
            handleError("Invalid total time for VOD stream");
            return;
        }

        ui->setTotalTime(totalTime);
        setWindowTitle("Video Player - Network Stream(VOD)");
    }
    else {
        ui->setSpeedComboBoxEnabled(false);
        ui->setProgressSliderEnabled(false);
        setWindowTitle("Video Player - Network Stream(LIVE)");
    }

    this->networkUrl = networkUrl;
    setupThreads();

    state = Loaded;
    QTimer::singleShot(50, this, [this]() {
        if (state == Loaded) {
            onPlayRequest();
        }
        });
}

void VideoPlayer::onPlayRequest() {
    if (state == Idle || state == Loading || state == Playing) {
        return;
    }

    ui->setPlay(true);

    if (state == Loaded) {
        if (videoPlayThread) videoPlayThread->start();
        if (audioPlayThread) audioPlayThread->start();
        if (videoDecoderThread) videoDecoderThread->start();
        if (audioDecoderThread) audioDecoderThread->start();
        if (demuxThread) demuxThread->start();
    }
    else if (state == Paused || state == Seeking || state == Finished) {
        if (videoPlayThread) videoPlayThread->resume();
        if (audioPlayThread) audioPlayThread->resume();
    }

    if (progressTimer) progressTimer->start();

    state = Playing;
}

void VideoPlayer::onPauseRequest() {
    if (state != Playing && state != Seeking) {
        return;
    }

    ui->setPlay(false);

    buffer->lock();
    if (videoPlayThread) videoPlayThread->pause();
    if (audioPlayThread) audioPlayThread->pause();
    buffer->unlock();
    if (progressTimer) progressTimer->stop();

    state = Paused;
}

void VideoPlayer::onStopRequest() {
    if (state == Idle || state == Loading) {
        return;
    }

    cleanup();
    setWindowTitle("Video Player");
    ui->resetUiState();
    state = Idle;
}

void VideoPlayer::onSeekRequest(int position) {
    if (MCTX()->mediaInput()->duration() <= 0) {
        return;
    }

    if (state == Idle || state == Loading || state == Loaded) {
        return;
    }

    if (state == Seeking) {
        return;
    }

    PlayState oldState = state;
    state = Seeking;

    buffer->lock();
    if (videoPlayThread) videoPlayThread->pause();
    if (audioPlayThread) audioPlayThread->pause();
    buffer->unlock();
    if (progressTimer) progressTimer->stop();

    int64_t totalTime = MCTX()->mediaInput()->duration();
    if (totalTime <= 0) {
        state = oldState;
        if (oldState == Playing) {
            if (videoPlayThread) videoPlayThread->resume();
            if (audioPlayThread) audioPlayThread->resume();
            if (progressTimer) progressTimer->start();
        }
        return;
    }

    position = qBound(0, position, 100);
    int64_t targetTime = (static_cast<int64_t>(position) * totalTime + 50) / 100;

    if (demuxThread) {
        demuxThread->seek(targetTime);
    }

    QTimer::singleShot(100, this, [this, oldState]() {
        if (state == Seeking) {
            if (oldState == Playing) {
                onPlayRequest();
            }
            else {
                state = oldState;
            }
        }
        });
}

void VideoPlayer::onSpeedChanged(float speed) {
    if (MCTX()->mediaInput()->duration() <= 0) {
        return;
    }

    if (state == Idle || state == Loading) {
        return;
    }

    if (speed <= 0.0f) {
        return;
    }

    if (videoPlayThread) videoPlayThread->setSpeed(speed);
    if (audioPlayThread) audioPlayThread->setSpeed(speed);
}

void VideoPlayer::onVolumeChanged(int volume) {
    if (state == Idle || state == Loading) {
        return;
    }

    volume = qBound(0, volume, 100);

    if (audioPlayThread) {
        audioPlayThread->setVolume(volume);
    }
}

void VideoPlayer::onUpdateProgress() {
    if (state != Playing) {
        return;
    }

    double currentTime = 0.0;
    if (videoPlayThread) {
        currentTime = videoPlayThread->getCurrentTime();
    }
    else if (audioPlayThread) {
        currentTime = audioPlayThread->getCurrentTime();
    }

    if (currentTime <= 0.0) {
        currentTime = 0.0;
    }

    ui->setCurrentTime(static_cast<int64_t>(currentTime));

    if (MCTX()->mediaInput()->duration() > 0) {
        int64_t totalTime = ui->getTotalTime();
        if (totalTime > 0) {
            int64_t currentTimeSec = static_cast<int64_t>(currentTime);
            if (currentTimeSec >= totalTime - 1) {
                if (state == Playing) {
                    state = Finished;
                    QTimer::singleShot(1000, this, [this]() {
                        if (state == Finished) {
                            onPlaybackFinished();
                        }
                        });
                }
            }
        }
    }
}

void VideoPlayer::onPlaybackFinished() {
    if (state != Finished) {
        return;
    }

    buffer->lock();
    if (videoPlayThread) videoPlayThread->pause();
    if (audioPlayThread) audioPlayThread->pause();
    buffer->unlock();
    if (progressTimer) progressTimer->stop();
    if (demuxThread) demuxThread->seek(0);

    ui->setPlay(false);

    QTimer::singleShot(100, this, [this]() {
        if (state == Finished) {
            ui->setCurrentTime(0);
            state = Paused;
        }
        });
}

void VideoPlayer::onErrorOccurred(const QString& errorMsg) {
    if (state != Idle) {
        handleError(errorMsg);
    }
}