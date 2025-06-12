#pragma once

#include <QMutex>
#include <QMutexLocker>
#include <cmath>
#include <deque>
#include <algorithm>
#include "SDK.h"

constexpr const double AV_SYNC_THRESHOLD_MIN = 0.008;
constexpr const double AV_SYNC_THRESHOLD_HALF = 0.025;
constexpr const double AV_SYNC_THRESHOLD_MAX = 0.08;
constexpr const double AV_NOSYNC_THRESHOLD = 5.0;
constexpr const double SYSTEM_DELAY_ESTIMATE = 0.002;
constexpr const double FINE_TUNE_RANGE = 0.005;
constexpr const size_t AV_SYNC_HISTORY_SIZE = 30;

constexpr const double DELAY_REDUCTION_FACTOR = 0.6;
constexpr const double DELAY_INCREASE_FACTOR = 1.4;
constexpr const double AUDIO_DELAY_IMPACT_SLOW = 0.7;
constexpr const double AUDIO_DELAY_IMPACT_FAST = 0.4;
constexpr const double AUDIO_DELAY_COMPENSATION = 0.25;

constexpr const double STABLE_VARIANCE_THRESHOLD = 0.0002;
constexpr const size_t MIN_STABLE_SAMPLES = 8;
constexpr const double DEFAULT_FRAME_DURATION = 1.0 / 30.0;

constexpr const int MAX_REASONABLE_DELAY_MS = 200;
constexpr const int MIN_REASONABLE_DELAY_MS = 0;
constexpr const double FALLBACK_TO_SYSTEM_THRESHOLD = 0.5;

struct Clock {
    double pts = 0.0;
    double duration = 0.0;
    double systemTime = 0.0;
    double lastSystemTime = 0.0;
    double presentationTime = 0.0;
    bool isValid = false;
    double speed = 1.0;
    double lastSpeed = 1.0;

    double avgDuration = DEFAULT_FRAME_DURATION;
    int updateCount = 0;

    void update(double newPts, double newDuration, double currentSystemTime, double currentSpeed) {
        bool speedChanged = std::fabs(speed - currentSpeed) > 0.001;

        lastSpeed = speed;
        pts = newPts;
        duration = newDuration;
        lastSystemTime = systemTime;
        systemTime = currentSystemTime;
        speed = currentSpeed;

        if (isValid && lastSystemTime > 0 && !speedChanged) {
            presentationTime = pts;
        }
        else {
            presentationTime = pts;
        }

        if (newDuration > 0 && newDuration < 10.0) {
            if (updateCount < 10) {
                avgDuration = (avgDuration * updateCount + newDuration) / (updateCount + 1);
                updateCount++;
            }
            else {
                avgDuration = avgDuration * 0.95 + newDuration * 0.05;
            }
        }

        isValid = (newPts != AV_NOPTS_VALUE);
    }

    void reset() {
        pts = duration = systemTime = lastSystemTime = presentationTime = 0.0;
        avgDuration = DEFAULT_FRAME_DURATION;
        updateCount = 0;
        isValid = false;
        speed = 1.0;
        lastSpeed = 1.0;
    }

    double getEffectiveDuration() const {
        double baseDuration = (duration > 0 && duration < 10.0) ? duration : avgDuration;
        return baseDuration / speed;
    }

    double getCurrentMediaTime(double currentSystemTime) const {
        if (!isValid) return 0.0;

        double elapsed = currentSystemTime - systemTime;

        if (std::fabs(speed - 1.0) < 0.001) {
            return pts + elapsed;
        }
        else if (std::fabs(speed - 2.0) < 0.001) {
            return pts + elapsed * 2.0;
        }
        else if (std::fabs(speed - 0.5) < 0.001) {
            return pts + elapsed * 0.5;
        }
        else {
            if (std::fabs(speed - 1.5) < 0.001) {
                return pts + (elapsed * 3.0) / 2.0;
            }
            else {
                return pts + elapsed * speed;
            }
        }
    }
};

class AVSyncManager {
public:
    AVSyncManager(const AVSyncManager&) = delete;
    AVSyncManager& operator=(const AVSyncManager&) = delete;
    AVSyncManager(AVSyncManager&&) = default;
    AVSyncManager& operator=(AVSyncManager&&) = default;

    AVSyncManager();
    ~AVSyncManager() = default;

    void reset();
    void pause();
    void resume();
    void setSpeed(double speed);

    int calculateVideoDelay(double pts, double duration);
    void updateAudioClock(double pts, double duration);

private:
    double getSystemClock() const;
    double calculateAVOffset() const;
    double calculatePredictedAudioTime() const;
    void resetClocks();
    int calculateBasicDelay(double duration) const;
    int calculateCorrectedDelay(double offset, double duration) const;
    int calculateSystemBasedDelay(double duration) const;
    int applyFineTune(int baseDelay, double offset) const;
    bool isDelayReasonable(int delay) const;
    int sanitizeDelay(int delay, double duration) const;
    bool isSystemStable() const;
    bool shouldUsePtsSync() const;
    void logSyncOffset(double offset);
    bool isValidPts(double pts) const;
    bool isValidDuration(double duration) const;
    double getSpeedAdjustedSystemDelay() const;

private:
    mutable QMutex mtx_;
    Clock videoClock_;
    Clock audioClock_;
    bool paused_;
    double speed_;
    int frameCount_;
    double lastOffset_;

    std::deque<double> syncLogs_;
    int consecutiveBadDelays_;
    double lastGoodOffset_;
    bool forceSystemMode_;
};