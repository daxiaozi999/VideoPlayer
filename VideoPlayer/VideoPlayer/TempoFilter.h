#pragma once

#include <QString>
#include <QMutex>
#include <stdio.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "SDK.h"

static constexpr float MIN_TEMPO       = 0.25f;
static constexpr float MAX_TEMPO       = 4.0f;
static constexpr float ATEMPO_MIN      = 0.5f;
static constexpr float ATEMPO_MAX      = 2.0f;
static constexpr int   MAX_TEMPO_NODES = 4;

static constexpr float COMMON_TEMPOS[] = {
    0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f
};

struct FilterNode {
    AVFilterContext* context = nullptr;
    float tempoValue = 1.0f;
    bool inUse = false;
};

struct TempoStrategy {
    float targetTempo = 1.0f;
    std::vector<float> chain = {};
};

class TempoFilter {
public:
    TempoFilter(const TempoFilter&) = delete;
    TempoFilter& operator=(const TempoFilter&) = delete;
    TempoFilter(TempoFilter&&) = delete;
    TempoFilter& operator=(TempoFilter&&) = delete;

    TempoFilter();
    ~TempoFilter();

    int initTempoFilter(int samplerate, int threads, AVRational timebase,
        const AVChannelLayout& chlayout, AVSampleFormat samplefmt);

    int setTempo(float tempo);
    int addFrame(AVFrame* srcFrame);
    int getFrame(AVFrame* dstFrame);

    bool isInitialized()    const;
    float getCurrentTempo() const;
    QString getLastError()  const;

    AVFilterContext* getBufferSource() const;
    AVFilterContext* getBufferSink() const;

    void flush();
    void cleanup();

private:
    int buildCompleteFilterChain(float tempo);
    int createFilterGraph();
    int createBufferSource();
    int createBufferSink();

    int createAndLinkTempoChain(const std::vector<float>& chain);
    int linkCompleteChain();

    std::vector<float> getTempoChain(float tempo);
    std::vector<float> calculateTempoChain(float targetTempo);
    TempoStrategy* findCachedStrategy(float tempo);
    void cacheStrategy(float tempo, const std::vector<float>& chain);
    bool isValidTempo(float tempo) const;

private:
    mutable QMutex mtx_;
    int threads_;
    int samplerate_;
    AVRational timebase_;
    AVChannelLayout chlayout_;
    AVSampleFormat samplefmt_;

    bool initialized_;
    float currentTempo_;
    QString lastError_;

    std::vector<std::unique_ptr<FilterNode>> tempoNodes_;
    std::unordered_map<int, std::unique_ptr<TempoStrategy>> strategyCache_;

    AVFilterGraph* filterGraph_;
    AVFilterContext* bufferSource_;
    AVFilterContext* bufferSink_;
};