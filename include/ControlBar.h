#pragma once

#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QFrame>
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QStringList>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "CustomSlider.h"

class ControlBar : public QFrame {
    Q_OBJECT

public:
    explicit ControlBar(QWidget* parent = nullptr);
    ~ControlBar() = default;

    CustomSlider* getProgressSlider()  const { return progressSlider; }
    QPushButton* getMenuButton()       const { return menuButton; }
    QPushButton* getPlayButton()       const { return playButton; }
    QPushButton* getPauseButton()      const { return pauseButton; }
    QPushButton* getStopButton()       const { return stopButton; }
    QLabel* getTimeLabel()             const { return timeLabel; }
    QComboBox* getSpeedComboBox()      const { return speedComboBox; }
    QPushButton* getVolumeButton()     const { return volumeButton; }
    QPushButton* getMuteButton()       const { return muteButton; }
    CustomSlider* getVolumeSlider()    const { return volumeSlider; }
    QPushButton* getFullscreenButton() const { return fullscreenButton; }

    int getCurrentProgress() const;
    int getCurrentVolume() const;
    float getCurrentSpeed() const;
    void setProgressSliderEnabled(bool enabled);
    void setSpeedComboBoxEnabled(bool enabled);
    void setPlayButtonVisible(bool visible);
    void setPauseButtonVisible(bool visible);
    void setVolumeButtonVisible(bool visible);
    void setMuteButtonVisible(bool visible);
    void setTimeLabel(const QString& currentTime, const QString& totalTime = "");
    void setProgress(int value);
    void setSpeed(float speed);
    void setVolume(int value);
    void setFullscreen(bool isFullscreen);

signals:
    void progressSliderPressed(int value);
    void progressSliderMoved(int value);
    void progressSliderReleased(int value);
    void menuClicked();
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void speedChanged(float speed);
    void volumeClicked();
    void muteClicked();
    void volumeSliderPressed(int value);
    void volumeSliderMoved(int value);
    void volumeSliderReleased(int value);
    void fullscreenClicked();

private slots:
    void onSpeedChanged(int index);

private:
    void setupUi();
    void setupProgressSlider();
    void setupControlsLayout();
    void setupLeftControls();
    void setupRightControls();
    void setupConnections();
    float speedTextToFloat(const QString& speedText) const;
    QPushButton* createControlButton(const QString& iconPath, const QString& toolTip);
    QString getControlButtonStyle() const;
    QString getSpeedComboBoxStyle() const;

private:
    QVBoxLayout* mainLayout;
    QHBoxLayout* controlsLayout;
    QWidget* leftWidget;
    QHBoxLayout* leftLayout;
    QWidget* rightWidget;
    QHBoxLayout* rightLayout;
    CustomSlider* progressSlider;
    QPushButton* menuButton;
    QPushButton* playButton;
    QPushButton* pauseButton;
    QPushButton* stopButton;
    QLabel* timeLabel;
    QComboBox* speedComboBox;
    QPushButton* volumeButton;
    QPushButton* muteButton;
    CustomSlider* volumeSlider;
    QPushButton* fullscreenButton;
};