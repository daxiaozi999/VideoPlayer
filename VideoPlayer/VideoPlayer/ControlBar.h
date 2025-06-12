#pragma once

#include <QWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QComboBox>
#include <QStringList>
#include "CustomSlider.h"

class ControlBar : public QFrame {
    Q_OBJECT

public:
    explicit ControlBar(QWidget* parent = nullptr);
    ~ControlBar() = default;

    CustomSlider* getProgressSlider()   const { return progressSlider; }
    QPushButton* getSettingsButton()    const { return settingsButton; }
    QPushButton* getPlayButton()        const { return playButton; }
    QPushButton* getPauseButton()       const { return pauseButton; }
    QLabel* getTimeLabel()              const { return timeLabel; }
    QComboBox* getSpeedComboBox()       const { return speedComboBox; }
    QPushButton* getVolumeButton()      const { return volumeButton; }
    QPushButton* getMuteButton()        const { return muteButton; }
    CustomSlider* getVolumeSlider()     const { return volumeSlider; }
    QPushButton* getFullscreenButton()  const { return fullscreenButton; }

    int getCurrentProgress() const;
    int getCurrentVolume() const;
    float getCurrentSpeed() const;

    void setProgressSliderEnabled(bool enable);
    void setSpeedComboBoxEnabled(bool enable);
    void setPlayButtonVisible(bool visible);
    void setPauseButtonVisible(bool visible);
    void setVolumeButtonVisible(bool visible);
    void setMuteButtonVisible(bool visible);

    void setTimeLabel(const QString& currentTime, const QString& totalTime = "");
    void setProgress(int value);
    void setProgressRange(int min, int max);
    void setPlaybackSpeed(float speed);
    void setVolume(int value);
    void setFullscreen(bool isFullscreen);

signals:
    void settingsClicked();
    void playClicked();
    void pauseClicked();
    void fullscreenClicked();

    void volumeButtonClicked();
    void muteButtonClicked();
    void volumeSliderClicked(int value);
    void volumeSliderMoved(int value);
    void volumeSliderReleased(int value);

    void progressSliderClicked(int value);
    void progressSliderMoved(int value);
    void progressSliderReleased(int value);

    void speedChanged(float speed);

private slots:
    void onSettingsClicked();
    void onPlayClicked();
    void onPauseClicked();
    void onFullscreenClicked();

    void onVolumeButtonClicked();
    void onMuteButtonClicked();
    void onVolumeSliderClicked(int value);
    void onVolumeSliderMoved(int value);
    void onVolumeSliderReleased(int value);

    void onProgressSliderClicked(int value);
    void onProgressSliderMoved(int value);
    void onProgressSliderReleased(int value);

    void onSpeedChanged(int index);

private:
    void setupUi();
    void connectSignals();
    bool validateComponents() const;

    void setControlBarStyle();
    void createMainLayout();
    void setupProgressSlider();
    void setupControlsLayout();
    void setupLeftControls();
    void setupRightControls();
    void addWidgetsToLayout();

    void createLeftWidget();
    void createLeftButtons();
    void createTimeLabel();
    void addLeftWidgetsToLayout();

    void createRightWidget();
    void createRightButtons();
    void createSpeedComboBox();
    void createVolumeSlider();
    void addRightWidgetsToLayout();

    QPushButton* createControlButton(const QString& iconPath, const QString& tooltip);
    QString getControlButtonStyle() const;
    QString getSpeedComboBoxStyle() const;

    float parseSpeedText(const QString& speedText) const;
    QString formatSpeedText(float speed) const;

private:
    QVBoxLayout* mainLayout;
    QHBoxLayout* controlsLayout;
    QWidget* leftWidget;
    QHBoxLayout* leftLayout;
    QWidget* rightWidget;
    QHBoxLayout* rightLayout;

    CustomSlider* progressSlider;
    QPushButton* settingsButton;
    QPushButton* playButton;
    QPushButton* pauseButton;
    QLabel* timeLabel;
    QComboBox* speedComboBox;
    QPushButton* volumeButton;
    QPushButton* muteButton;
    CustomSlider* volumeSlider;
    QPushButton* fullscreenButton;
};