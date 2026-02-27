#pragma once

#include <memory>
#include <QTimer>
#include <QWidget>
#include <QFileInfo>
#include <QMessageBox>
#include <QtWidgets/QMainWindow>
#include "VideoPlayerUi.h"
#include "MediaBuffer.h"
#include "MediaContext.h"
#include "DemuxThread.h"
#include "VideoPlayThread.h"
#include "AudioPlayThread.h"
#include "VideoDecodeThread.h"
#include "AudioDecodeThread.h"

class VideoPlayer : public QMainWindow {
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget* parent = nullptr);
    ~VideoPlayer();

private slots:
    void onLoadLocalVideo(const QString& filePath);
    void onLoadNetworkVideo(const QString& networkUrl);
    void onPlayRequest();
    void onPauseRequest();
    void onStopRequest();
    void onSeekRequest(int position);
    void onSpeedChanged(float speed);
    void onVolumeChanged(int volume);
    void onUpdateProgress();
    void onPlaybackFinished();
    void onErrorOccurred(const QString& error);

private:
    void setupUi();
    void setupConnections();
    void setupThreads();
    void setupThreadConnections();
    void handleError(const QString& error);
    void showErrorMessage(const QString& error);
    void cleanup();
    void cleanupThread(QThread* thread);

private:
    VideoPlayerUi* ui;

    enum PlayState {
        Idle,
        Loading,
        Loaded,
        Playing,
        Paused,
        Seeking,
        Finished,
    } state;

    QString filePath;
    QString networkUrl;

    std::shared_ptr<MediaBuffer> buffer;

    QTimer* progressTimer;
    DemuxThread* demuxThread;
    VideoDecodeThread* videoDecoderThread;
    AudioDecodeThread* audioDecoderThread;
    VideoPlayThread* videoPlayThread;
    AudioPlayThread* audioPlayThread;
};