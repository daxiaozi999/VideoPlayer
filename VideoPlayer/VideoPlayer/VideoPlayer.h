#pragma once

#include <QtWidgets/QMainWindow>
#include <QTimer>
#include <QMessageBox>
#include <QFileInfo>
#include "VideoPlayerWidget.h"
#include "MediaBuffer.h"
#include "MediaContext.h"
#include "DemuxThread.h"
#include "VideoDecoderThread.h"
#include "AudioDecoderThread.h"
#include "VideoPlayThread.h"
#include "AudioPlayThread.h"

class VideoPlayer : public QMainWindow {
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget* parent = nullptr);
    ~VideoPlayer();

private slots:
    void onLoadLocalVideo(const QString& file);
    void onLoadNetworkVideo(const QString& url);
    void onPlayRequested();
    void onPauseRequested();
    void onSeekRequested(int position);
    void onSpeedChanged(float speed);
    void onVolumeChanged(int volume);
    void onProgressUpdate();
    void onPlaybackFinished();
    void onErrorOccurred(const QString& errorMsg);

private:
    void setupUi();
    void setupWidget();
    void setupConnections();
    void resetWidgetState();
    void updateWindowTitle(const QString& str);
    void setupThreads();
    void setupThreadConnections();

    void showErrorMessage(const QString& errorMsg);
    void cleanup();
    void cleanupThread(QThread* thread);

private:
    VideoPlayerWidget* widget;
    QString filePath;
    QString streamUrl;

    enum PlayState {
        Error = -1,
        Idle,
        Loaded,
        Playing,
        Paused,
        Seeking,
        Finished,
    } state;

    std::shared_ptr<MediaBuffer> buffer;

    QTimer* progressTimer;
    DemuxThread* demuxThread;
    VideoDecoderThread* videoDecoderThread;
    AudioDecoderThread* audioDecoderThread;
    VideoPlayThread* videoPlayThread;
    AudioPlayThread* audioPlayThread;
};