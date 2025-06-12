#include "TempoFilter.h"

TempoFilter::TempoFilter()
    : threads_(1)
    , samplerate_(48000)
    , timebase_({ 1, 48000 })
    , chlayout_(AV_CHANNEL_LAYOUT_STEREO)
    , samplefmt_(AV_SAMPLE_FMT_S16)
    , initialized_(false)
    , currentTempo_(1.0f)
    , lastError_("")
    , filterGraph_(nullptr)
    , bufferSource_(nullptr)
    , bufferSink_(nullptr) {
}

TempoFilter::~TempoFilter() {
    cleanup();
}

int TempoFilter::initTempoFilter(int samplerate, int threads, AVRational timebase,
    const AVChannelLayout& chlayout, AVSampleFormat samplefmt) {
    QMutexLocker locker(&mtx_);

    if (initialized_) {
        cleanup();
    }

    threads_    = threads < 0 ? 1 : threads;
    samplerate_ = samplerate;
    timebase_   = timebase;
    samplefmt_  = samplefmt;
    av_channel_layout_copy(&chlayout_, &chlayout);

    for (float tempo : COMMON_TEMPOS) {
        std::vector<float> chain = calculateTempoChain(tempo);
        cacheStrategy(tempo, chain);
    }

    return buildCompleteFilterChain(1.0f);
}

int TempoFilter::setTempo(float tempo) {
    QMutexLocker locker(&mtx_);

    if (!initialized_) {
        lastError_ = "Filter not initialized";
        return AVERROR(EINVAL);
    }

    if (!isValidTempo(tempo)) {
        lastError_ = QString("Invalid tempo: %1").arg(tempo);
        return AVERROR(EINVAL);
    }

    if (std::abs(tempo - currentTempo_) < 0.001f) {
        return 0;
    }

    return buildCompleteFilterChain(tempo);
}

int TempoFilter::addFrame(AVFrame* srcFrame) {
    QMutexLocker locker(&mtx_);

    if (!initialized_ || !srcFrame) {
        return AVERROR(EINVAL);
    }

    int ret = av_buffersrc_add_frame(bufferSource_, srcFrame);
    if (ret < 0) {
        lastError_ = QString("Failed to add frame to buffer source: %1").arg(ret);
    }
    return ret;
}

int TempoFilter::getFrame(AVFrame* dstFrame) {
    QMutexLocker locker(&mtx_);

    if (!initialized_ || !dstFrame) {
        return AVERROR(EINVAL);
    }

    int ret = av_buffersink_get_frame(bufferSink_, dstFrame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        lastError_ = QString("Failed to get frame from buffer sink: %1").arg(ret);
    }
    return ret;
}

bool TempoFilter::isInitialized() const {
    QMutexLocker locker(&mtx_);
    return initialized_;
}

float TempoFilter::getCurrentTempo() const {
    QMutexLocker locker(&mtx_);
    return currentTempo_;
}

QString TempoFilter::getLastError() const {
    QMutexLocker locker(&mtx_);
    return lastError_;
}

AVFilterContext* TempoFilter::getBufferSource() const {
    QMutexLocker locker(&mtx_);
    return bufferSource_;
}

AVFilterContext* TempoFilter::getBufferSink() const {
    QMutexLocker locker(&mtx_);
    return bufferSink_;
}

void TempoFilter::flush() {
    QMutexLocker locker(&mtx_);

    if (!initialized_) return;

    if (bufferSource_) {
        av_buffersrc_add_frame(bufferSource_, nullptr);
    }
}

void TempoFilter::cleanup() {
    tempoNodes_.clear();
    strategyCache_.clear();

    bufferSource_ = nullptr;
    bufferSink_ = nullptr;

    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
        filterGraph_ = nullptr;
    }

    av_channel_layout_uninit(&chlayout_);
    chlayout_ = AV_CHANNEL_LAYOUT_STEREO;

    initialized_ = false;
    currentTempo_ = 1.0f;
    lastError_.clear();
}

int TempoFilter::buildCompleteFilterChain(float tempo) {
    tempoNodes_.clear();
    bufferSource_ = nullptr;
    bufferSink_ = nullptr;

    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
        filterGraph_ = nullptr;
    }

    std::vector<float> chain = getTempoChain(tempo);
    if (chain.empty()) {
        lastError_ = QString("Failed to calculate tempo chain for %1").arg(tempo);
        return AVERROR(EINVAL);
    }

    int ret = createFilterGraph();
    if (ret < 0) {
        lastError_ = QString("Failed to create filter graph: %1").arg(ret);
        return ret;
    }

    ret = createBufferSource();
    if (ret < 0) {
        lastError_ = QString("Failed to create buffer source: %1").arg(ret);
        cleanup();
        return ret;
    }

    ret = createBufferSink();
    if (ret < 0) {
        lastError_ = QString("Failed to create buffer sink: %1").arg(ret);
        cleanup();
        return ret;
    }

    ret = createAndLinkTempoChain(chain);
    if (ret < 0) {
        cleanup();
        return ret;
    }

    ret = avfilter_graph_config(filterGraph_, nullptr);
    if (ret < 0) {
        lastError_ = QString("Failed to configure filter graph: %1").arg(ret);
        cleanup();
        return ret;
    }

    currentTempo_ = tempo;
    initialized_ = true;
    return 0;
}

int TempoFilter::createFilterGraph() {
    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) {
        return AVERROR(ENOMEM);
    }

    filterGraph_->nb_threads = threads_;
    return 0;
}

int TempoFilter::createBufferSource() {
    if (!filterGraph_) {
        return AVERROR(EINVAL);
    }

    AVBPrint bp;
    av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);
    av_channel_layout_describe_bprint(&chlayout_, &bp);

    char* chlayoutStr = nullptr;
    av_bprint_finalize(&bp, &chlayoutStr);
    if (!chlayoutStr) {
        return AVERROR(ENOMEM);
    }

    char args[512];
    snprintf(args, sizeof(args),
        "sample_rate=%d:sample_fmt=%s:time_base=%d/%d:channel_layout=%s",
        samplerate_,
        av_get_sample_fmt_name(samplefmt_),
        1, samplerate_, chlayoutStr);

    int ret = avfilter_graph_create_filter(&bufferSource_, avfilter_get_by_name("abuffer"), "ffmpeg_abuffer",
                                            args, nullptr, filterGraph_);
    av_free(chlayoutStr);

    if (ret < 0) {
        bufferSource_ = nullptr;
        return ret;
    }

    return 0;
}

int TempoFilter::createBufferSink() {
    if (!filterGraph_) {
        return AVERROR(EINVAL);
    }

    int ret = avfilter_graph_create_filter(&bufferSink_, avfilter_get_by_name("abuffersink"), "ffmpeg_abuffersink",
                                            nullptr, nullptr, filterGraph_);
    if (ret < 0) {
        bufferSink_ = nullptr;
        return ret;
    }

    AVBPrint bp;
    av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);
    av_channel_layout_describe_bprint(&chlayout_, &bp);

    char* chlayoutStr = nullptr;
    av_bprint_finalize(&bp, &chlayoutStr);
    if (!chlayoutStr) {
        return AVERROR(ENOMEM);
    }

    enum AVSampleFormat samplefmts[]{ samplefmt_, AV_SAMPLE_FMT_NONE };
    int samplerates[]{ samplerate_, -1 };

    ret = 0;
    ret |= av_opt_set_int_list(bufferSink_, "sample_fmts", samplefmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    ret |= av_opt_set(bufferSink_, "ch_layouts", chlayoutStr, AV_OPT_SEARCH_CHILDREN);
    ret |= av_opt_set_int_list(bufferSink_, "sample_rates", samplerates, -1, AV_OPT_SEARCH_CHILDREN);

    av_free(chlayoutStr);

    if (ret < 0) {
        return ret;
    }

    return 0;
}

int TempoFilter::createAndLinkTempoChain(const std::vector<float>& chain) {
    if (chain.size() == 1 && std::abs(chain[0] - 1.0f) < 0.001f) {
        int ret = avfilter_link(bufferSource_, 0, bufferSink_, 0);
        if (ret < 0) {
            lastError_ = QString("Failed to link direct connection: %1").arg(ret);
            return ret;
        }
        return 0;
    }

    tempoNodes_.clear();
    tempoNodes_.reserve(chain.size());

    for (size_t i = 0; i < chain.size() && i < MAX_TEMPO_NODES; ++i) {
        auto node = std::make_unique<FilterNode>();

        char name[64];
        snprintf(name, sizeof(name), "atempo_%zu", i);

        char tempoStr[32];
        snprintf(tempoStr, sizeof(tempoStr), "%.3f", chain[i]);

        int ret = avfilter_graph_create_filter(&node->context, avfilter_get_by_name("atempo"),
            name, tempoStr, nullptr, filterGraph_);
        if (ret < 0) {
            lastError_ = QString("Failed to create tempo node %1: %2").arg(i).arg(ret);
            return ret;
        }

        node->tempoValue = chain[i];
        node->inUse = true;
        tempoNodes_.push_back(std::move(node));
    }

    return linkCompleteChain();
}

int TempoFilter::linkCompleteChain() {
    if (tempoNodes_.empty()) {
        return AVERROR(EINVAL);
    }

    int ret = avfilter_link(bufferSource_, 0, tempoNodes_[0]->context, 0);
    if (ret < 0) {
        lastError_ = QString("Failed to link buffer to first atempo: %1").arg(ret);
        return ret;
    }

    for (size_t i = 1; i < tempoNodes_.size(); ++i) {
        ret = avfilter_link(tempoNodes_[i - 1]->context, 0, tempoNodes_[i]->context, 0);
        if (ret < 0) {
            lastError_ = QString("Failed to link atempo %1 to %2: %3")
                .arg(i - 1).arg(i).arg(ret);
            return ret;
        }
    }

    ret = avfilter_link(tempoNodes_.back()->context, 0, bufferSink_, 0);
    if (ret < 0) {
        lastError_ = QString("Failed to link last atempo to sink: %1").arg(ret);
        return ret;
    }

    return 0;
}

std::vector<float> TempoFilter::getTempoChain(float tempo) {
    TempoStrategy* strategy = findCachedStrategy(tempo);
    if (strategy) {
        return strategy->chain;
    }

    std::vector<float> chain = calculateTempoChain(tempo);
    cacheStrategy(tempo, chain);
    return chain;
}

std::vector<float> TempoFilter::calculateTempoChain(float targetTempo) {
    std::vector<float> chain;

    if (std::abs(targetTempo - 1.0f) < 0.001f) {
        chain.push_back(1.0f);
        return chain;
    }

    float remaining = targetTempo;

    while (std::abs(remaining - 1.0f) > 0.001f && chain.size() < MAX_TEMPO_NODES) {
        if (remaining >= ATEMPO_MIN && remaining <= ATEMPO_MAX) {
            chain.push_back(remaining);
            break;
        }
        else if (remaining > ATEMPO_MAX) {
            chain.push_back(ATEMPO_MAX);
            remaining /= ATEMPO_MAX;
        }
        else if (remaining < ATEMPO_MIN) {
            chain.push_back(ATEMPO_MIN);
            remaining /= ATEMPO_MIN;
        }
    }

    return chain;
}

TempoStrategy* TempoFilter::findCachedStrategy(float tempo) {
    int key = static_cast<int>(tempo * 1000);
    auto it = strategyCache_.find(key);
    return (it != strategyCache_.end()) ? it->second.get() : nullptr;
}

void TempoFilter::cacheStrategy(float tempo, const std::vector<float>& chain) {
    int key = static_cast<int>(tempo * 1000);
    auto strategy = std::make_unique<TempoStrategy>();
    strategy->targetTempo = tempo;
    strategy->chain = chain;
    strategyCache_[key] = std::move(strategy);
}

bool TempoFilter::isValidTempo(float tempo) const {
    return tempo >= MIN_TEMPO && tempo <= MAX_TEMPO;
}