#include "AVSyncManager.h"

AVSyncManager::AVSyncManager()
    : paused_(false)
    , speed_(1.0)
    , frameCount_(0)
    , lastOffset_(0.0)
    , syncLogs_({})
    , consecutiveBadDelays_(0)
    , lastGoodOffset_(0.0)
    , forceSystemMode_(false) {
}

void AVSyncManager::reset() {
    QMutexLocker locker(&mtx_);
    resetClocks();
    frameCount_ = 0;
    lastOffset_ = 0.0;
    syncLogs_.clear();
    consecutiveBadDelays_ = 0;
    lastGoodOffset_ = 0.0;
    forceSystemMode_ = false;
}

void AVSyncManager::pause() {
    QMutexLocker locker(&mtx_);
    paused_ = true;
}

void AVSyncManager::resume() {
    QMutexLocker locker(&mtx_);
    paused_ = false;
}

void AVSyncManager::setSpeed(double speed) {
    QMutexLocker locker(&mtx_);
    if (std::fabs(speed_ - speed) < 0.001) return;

    double oldSpeed = speed_;
    speed_ = speed;

    forceSystemMode_ = false;
    consecutiveBadDelays_ = 0;

    syncLogs_.clear();

    if (std::fabs(speed - std::round(speed)) > 0.001) {
        lastGoodOffset_ = 0.0;
    }
}

void AVSyncManager::updateAudioClock(double pts, double duration) {
    QMutexLocker locker(&mtx_);
    if (isValidPts(pts) && isValidDuration(duration)) {
        audioClock_.update(pts, duration, getSystemClock(), speed_);
    }
}

int AVSyncManager::calculateVideoDelay(double pts, double duration) {
    QMutexLocker locker(&mtx_);

    if (paused_) {
        return 0;
    }

    double effectiveDuration = duration;
    if (!isValidDuration(duration)) {
        effectiveDuration = videoClock_.avgDuration;
    }

    if (isValidPts(pts)) {
        videoClock_.update(pts, effectiveDuration, getSystemClock(), speed_);
    }

    int delay = 0;

    if (!audioClock_.isValid || forceSystemMode_ || !shouldUsePtsSync()) {
        delay = calculateSystemBasedDelay(effectiveDuration);
    }
    else {
        double offset = calculateAVOffset();
        double absOffset = std::fabs(offset);

        logSyncOffset(offset);

        if (absOffset > FALLBACK_TO_SYSTEM_THRESHOLD) {
            delay = calculateSystemBasedDelay(effectiveDuration);
            consecutiveBadDelays_++;

            if (consecutiveBadDelays_ > 5) {
                forceSystemMode_ = true;
            }
        }
        else {
            consecutiveBadDelays_ = 0;
            lastGoodOffset_ = offset;

            if (absOffset < AV_SYNC_THRESHOLD_MIN) {
                delay = calculateBasicDelay(effectiveDuration);
            }
            else if (absOffset < AV_SYNC_THRESHOLD_HALF) {
                delay = calculateBasicDelay(effectiveDuration) * 0.7;
            }
            else if (absOffset < AV_SYNC_THRESHOLD_MAX) {
                delay = calculateCorrectedDelay(offset, effectiveDuration);
            }
            else if (absOffset > AV_NOSYNC_THRESHOLD) {
                delay = calculateBasicDelay(effectiveDuration);
            }
            else {
                delay = calculateCorrectedDelay(offset, effectiveDuration);
            }
        }
    }

    if (!forceSystemMode_ && audioClock_.isValid) {
        double currentOffset = lastGoodOffset_;
        delay = applyFineTune(delay, currentOffset);
    }

    delay = sanitizeDelay(delay, effectiveDuration);

    frameCount_++;
    return delay;
}

int AVSyncManager::calculateSystemBasedDelay(double duration) const {
    double targetDelay;

    if (std::fabs(speed_ - 1.0) < 0.001) {
        targetDelay = duration;
    }
    else if (std::fabs(speed_ - 2.0) < 0.001) {
        targetDelay = duration * 0.5;
    }
    else if (std::fabs(speed_ - 0.5) < 0.001) {
        targetDelay = duration * 2.0;
    }
    else if (std::fabs(speed_ - 1.5) < 0.001) {
        targetDelay = (duration * 2.0) / 3.0;
    }
    else {
        targetDelay = duration / speed_;
    }

    targetDelay -= getSpeedAdjustedSystemDelay();
    targetDelay = std::max(0.0, targetDelay);

    return static_cast<int>(targetDelay * 1000);
}

bool AVSyncManager::shouldUsePtsSync() const {
    if (!audioClock_.isValid || !videoClock_.isValid) {
        return false;
    }

    if (isSystemStable() && std::fabs(lastGoodOffset_) < AV_SYNC_THRESHOLD_MAX) {
        return true;
    }

    if (consecutiveBadDelays_ > 3) {
        return false;
    }

    return true;
}

double AVSyncManager::calculateAVOffset() const {
    if (!audioClock_.isValid || !videoClock_.isValid) {
        return 0.0;
    }

    double currentTime = getSystemClock();
    double audioCurrentTime = audioClock_.getCurrentMediaTime(currentTime);
    double videoCurrentTime = videoClock_.getCurrentMediaTime(currentTime);

    return audioCurrentTime - videoCurrentTime;
}

double AVSyncManager::calculatePredictedAudioTime() const {
    if (!audioClock_.isValid) {
        return 0.0;
    }

    double currentTime = getSystemClock();
    return audioClock_.getCurrentMediaTime(currentTime);
}

int AVSyncManager::calculateBasicDelay(double duration) const {
    double actualDuration;

    if (std::fabs(speed_ - 1.0) < 0.001) {
        actualDuration = duration;
    }
    else if (std::fabs(speed_ - 2.0) < 0.001) {
        actualDuration = duration * 0.5;
    }
    else if (std::fabs(speed_ - 0.5) < 0.001) {
        actualDuration = duration * 2.0;
    }
    else if (std::fabs(speed_ - 1.5) < 0.001) {
        actualDuration = (duration * 2.0) / 3.0;
    }
    else {
        actualDuration = duration / speed_;
    }

    double baseDelay = actualDuration - getSpeedAdjustedSystemDelay();
    return static_cast<int>(std::max(0.0, baseDelay) * 1000);
}

int AVSyncManager::calculateCorrectedDelay(double offset, double duration) const {
    double adjustedDelay;

    if (std::fabs(speed_ - 1.0) < 0.001) {
        adjustedDelay = duration;
    }
    else if (std::fabs(speed_ - 2.0) < 0.001) {
        adjustedDelay = duration * 0.5;
    }
    else if (std::fabs(speed_ - 0.5) < 0.001) {
        adjustedDelay = duration * 2.0;
    }
    else if (std::fabs(speed_ - 1.5) < 0.001) {
        adjustedDelay = (duration * 2.0) / 3.0;
    }
    else {
        adjustedDelay = duration / speed_;
    }

    if (offset > 0) {
        adjustedDelay *= DELAY_REDUCTION_FACTOR;
    }
    else {
        adjustedDelay *= DELAY_INCREASE_FACTOR;
    }

    adjustedDelay -= getSpeedAdjustedSystemDelay();
    return static_cast<int>(std::max(0.0, adjustedDelay) * 1000);
}

int AVSyncManager::applyFineTune(int baseDelay, double offset) const {
    if (std::fabs(offset) < FINE_TUNE_RANGE) {
        return baseDelay;
    }

    if (!isSystemStable()) {
        return baseDelay;
    }

    int adjustment = 0;
    double absOffset = std::fabs(offset);

    if (absOffset < AV_SYNC_THRESHOLD_HALF) {
        double adjustmentFactor = 150.0 / speed_;

        if (offset > 0) {
            adjustment = -static_cast<int>(offset * adjustmentFactor);
        }
        else {
            adjustment = static_cast<int>(-offset * adjustmentFactor);
        }

        int maxAdjustment = static_cast<int>(10.0 / speed_);
        adjustment = qBound(-maxAdjustment, adjustment, maxAdjustment);
    }

    return baseDelay + adjustment;
}

bool AVSyncManager::isDelayReasonable(int delay) const {
    return delay >= MIN_REASONABLE_DELAY_MS && delay <= MAX_REASONABLE_DELAY_MS;
}

int AVSyncManager::sanitizeDelay(int delay, double duration) const {
    int maxDelay = static_cast<int>((duration / speed_) * 2000);
    int minDelay = 0;

    delay = qBound(minDelay, delay, std::min(maxDelay, MAX_REASONABLE_DELAY_MS));
    return delay;
}

void AVSyncManager::logSyncOffset(double offset) {
    syncLogs_.push_back(offset);

    if (syncLogs_.size() > AV_SYNC_HISTORY_SIZE) {
        syncLogs_.pop_front();
    }

    lastOffset_ = offset;
}

bool AVSyncManager::isSystemStable() const {
    if (syncLogs_.size() < MIN_STABLE_SAMPLES) {
        return false;
    }

    double sum = 0.0;
    for (double offset : syncLogs_) {
        sum += offset;
    }
    double mean = sum / syncLogs_.size();

    double variance = 0.0;
    for (double offset : syncLogs_) {
        double diff = offset - mean;
        variance += diff * diff;
    }
    variance /= syncLogs_.size();

    double adjustedThreshold = STABLE_VARIANCE_THRESHOLD * speed_;
    return variance < adjustedThreshold;
}

double AVSyncManager::getSystemClock() const {
    return av_gettime() / 1000000.0;
}

void AVSyncManager::resetClocks() {
    audioClock_.reset();
    videoClock_.reset();
}

bool AVSyncManager::isValidPts(double pts) const {
    return pts != AV_NOPTS_VALUE && std::isfinite(pts) && pts >= 0;
}

bool AVSyncManager::isValidDuration(double duration) const {
    return duration > 0.0 && std::isfinite(duration) && duration < 10.0;
}

double AVSyncManager::getSpeedAdjustedSystemDelay() const {
    if (std::fabs(speed_ - 1.0) < 0.001) {
        return SYSTEM_DELAY_ESTIMATE;
    }
    else if (std::fabs(speed_ - 2.0) < 0.001) {
        return SYSTEM_DELAY_ESTIMATE * 0.5;
    }
    else if (std::fabs(speed_ - 0.5) < 0.001) {
        return SYSTEM_DELAY_ESTIMATE * 2.0;
    }
    else if (std::fabs(speed_ - 1.5) < 0.001) {
        return (SYSTEM_DELAY_ESTIMATE * 2.0) / 3.0;
    }
    else {
        return SYSTEM_DELAY_ESTIMATE / speed_;
    }
}