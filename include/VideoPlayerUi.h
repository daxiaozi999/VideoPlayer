#pragma once

#include <QTimer>
#include <QEvent>
#include <QWidget>
#include <QMimeData>
#include <QMimeType>
#include <QFileInfo>
#include <QKeyEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QMimeDatabase>
#include <QDragEnterEvent>
#include <QAbstractItemView>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include "MenuDialog.h"
#include "ControlBar.h"
#include "YUVRenderer.h"
#include "CustomSlider.h"

class VideoPlayerUi : public QWidget {
    Q_OBJECT

public:
    explicit VideoPlayerUi(QWidget* parent = nullptr);
    ~VideoPlayerUi() = default;

    YUVRenderer* getVideoRenderer() const { return videoRenderer; }
    QString getFilePath()           const { return filePath; }
    QString getNetworkUrl()         const { return networkUrl; }
    int64_t getTotalTime()          const { return totalTime; }
    int64_t getCurrentTime()        const { return currentTime; }
    bool isPlay()                   const { return play; }
    bool isMuted()                  const { return muted; }
    bool isFullscreen()             const { return fullscreen; }
    float getSpeed()                const { return speed; }
    int getProgress()               const { return progress; }
    int getVolume()                 const { return volume; }

    void setTotalTime(int64_t totalTime);
    void setCurrentTime(int64_t currentTime);
    void setPlay(bool play);
    void setMuted(bool muted);
    void setFullscreen(bool fullscreen);
    void setSpeed(float speed);
    void setProgress(int progress);
    void setVolume(int volume);
    void setProgressSliderEnabled(bool enabled);
    void setSpeedComboBoxEnabled(bool enabled);
    void resetUiState();

signals:
    void loadLocalVideo(const QString& filePath);
    void loadNetworkVideo(const QString& networkUrl);
    void playRequest();
    void pauseRequest();
    void stopRequest();
    void seekRequest(int position);
    void speedChanged(float speed);
    void volumeChanged(int volume);

protected:
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void onMenuClicked();
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onSpeedChanged(float speed);
    void onVolumeClicked();
    void onMuteClicked();
    void onFullscreenClicked();
    void onProgressSliderMoved(int value);
    void onProgressSliderReleased(int value);
    void onVolumeSliderMoved(int value);
    void onVolumeSliderReleased(int value);
    void onAutoHideTimeout();
    void onHidePreviewLabel();

private:
    void setupUi();
    void setupConnections();
    void setupTimers();
    void setupPreviewLabels();
    void updatePlayState();
    void updateTimeLabel();
    void updateVolumeButton();
    void showControlBar();
    void hideControlBar(int duration);
    void resetAutoHideTimer();
    void stopAutoHideTimer();
    void hideControlsAndCursor();
    void updateControlBarVisible();
    bool handleControlBarEvent(QEvent* event);
    bool handleSpeedComboBoxEvent(QEvent* event);
    bool handleSpeedComboBoxViewEvent(QEvent* event);
    void hidePreviewLabel();
    void showTimePreview(int progress);
    void showVolumePreview(int volume);
    QLabel* createPreviewLabel(const QString& styleSheet);
    bool isValidVideoFile(const QString& filePath) const;
    QString formatTime(int64_t seconds) const;

private:
    QVBoxLayout* mainLayout;
    ControlBar* controlBar;
    YUVRenderer* videoRenderer;
    QPropertyAnimation* fadeAnimation;
    QGraphicsOpacityEffect* opacityEffect;
    QLabel* timePreviewLabel;
    QLabel* volumePreviewLabel;
    QTimer* autoHideTimer;
    QTimer* previewHideTimer;

    QString filePath;
    QString networkUrl;
    int64_t totalTime;
    int64_t currentTime;
    bool play;
    bool muted;
    bool fullscreen;
    float speed;
    int progress;
    int volume;
    int autoHideDelay;
    bool progressMoved;
    bool volumeMoved;
    bool mouseInWindow;
    bool mouseInControlBar;
    bool mouseInSpeedComboBox;
};