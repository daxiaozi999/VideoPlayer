#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QComboBox>
#include <QEasingCurve>
#include <QAbstractItemView>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include "CustomSlider.h"
#include "ControlBar.h"
#include "SettingsDialog.h"
#include "YUVRenderer.h"

class VideoPlayerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);
    ~VideoPlayerWidget() = default;

    YUVRenderer* getVideoArea()     const { return videoArea; }
    QString getCurrentFilePath()    const { return currentFilePath; }
    QString getCurrentUrl()         const { return currentUrl; }
    bool isPlaying()                const { return playing; }
    int64_t getTotalTime()          const { return totalTime; }
    int64_t getCurrentTime()        const { return currentTime; }
    float getCurrentSpeed()         const { return speed; }
    int getCurrentProgress()        const { return currentProgress; }
    int getCurrentVolume()          const { return currentVolume; }
    bool isMuted()                  const { return muted; }
    bool isFullscreen()             const { return fullscreen; }

    void setPlaying(bool isPlaying);
    void setTotalTime(int64_t totalTime);
    void setCurrentTime(int64_t currentSeconds);
    void setSpeed(float speed);
    void setVolume(int volume);
    void setMute(bool muted);
    void setFullscreen(bool isFullscreen);
    void setAutoHideEnabled(bool enabled);
    void setHideDelay(int milliseconds);
    void setProgress(int progress);
    void setProgressSliderEnabled(bool enable);
    void setSpeedComboBoxEnabled(bool enable);

signals:
    void loadLocalVideo(const QString& filePath);
    void loadNetworkVideo(const QString& url);
    void playRequested();
    void pauseRequested();
    void seekRequested(int position);
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
    void onSettingsClicked();
    void onPlayClicked();
    void onPauseClicked();
    void onVolumeButtonClicked();
    void onMuteButtonClicked();
    void onFullscreenClicked();
    void onVolumeSliderClicked(int value);
    void onVolumeSliderMoved(int value);
    void onVolumeSliderReleased(int value);
    void onProgressSliderClicked(int value);
    void onProgressSliderMoved(int value);
    void onProgressSliderReleased(int value);
    void onSpeedChanged(float speed);
    void onHideControlBar();
    void onHideCursor();
    void onHidePreviewLabels();

private:
    void setupUi();
    void setupConnections();
    void setupTimers();
    void setupPreviewLabels();

    void showControlBar();
    void hideControlBar();
    void hideControlBarDelayed();
    void hideControlBarImmediately();
    void hideControlBarWithDuration(int duration);
    void updateControlBarVisibility();
    bool isControlBarInteractionActive() const;

    void updateMouseState(bool mouseInWidget);
    void resetCursorTimer();
    void updateCursorVisibility();

    bool handleControlBarEvent(QEvent* event);
    bool handleVideoAreaEvent(QEvent* event);
    bool handleSpeedComboBoxEvent(QEvent* event);
    bool handleSpeedComboBoxViewEvent(QEvent* event);

    bool isValidVideoFile(const QString& filePath) const;
    void loadDroppedVideo(const QString& filePath);

    void updatePlayPauseButtons();
    void updateVolumeButtons();
    void updateTimeLabel();

    void showVolumePreview(int volume);
    void showTimePreview(int progress);
    void hideAllPreviewLabels();
    QLabel* createPreviewLabel(const QString& styleSheet);
    void positionVolumePreviewLabel(QLabel* label);
    void positionTimePreviewLabel(QLabel* label);

    QString formatTime(int64_t seconds) const;
    int64_t calculatePreviewTime(int progress) const;
    void cleanupFadeAnimation();

private:
    QVBoxLayout* mainLayout;
    YUVRenderer* videoArea;
    ControlBar* controlBar;
    QGraphicsOpacityEffect* opacityEffect;
    QPropertyAnimation* fadeAnimation;

    QLabel* volumePreviewLabel;
    QLabel* timePreviewLabel;
    QLabel* progressPreviewLabel;

    QTimer* hideTimer;
    QTimer* cursorHideTimer;
    QTimer* previewHideTimer;

    QString currentFilePath;
    QString currentUrl;
    bool playing;
    int64_t totalTime;
    int64_t currentTime;
    float speed;
    int currentProgress;
    int currentVolume;
    int volumeBeforeMute;
    bool muted;
    bool fullscreen;

    int hideDelay;
    int cursorHideDelay;
    bool autoHideEnabled;
    bool mouseInWidget;
    bool isMouseOverControlBar;
    bool isMouseOverSpeedComboBox;
    bool controlBarVisible;
    QMimeDatabase mimeDatabase;
};