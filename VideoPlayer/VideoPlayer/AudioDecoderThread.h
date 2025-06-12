#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <atomic>
#include <memory>
#include "SDK.h"
#include "MediaBuffer.h"
#include "MediaContext.h"

class AudioDecoderThread : public QThread {
	Q_OBJECT

public:
	AudioDecoderThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
	~AudioDecoderThread();

	bool init();
	void start();
	void stop();

public slots:
	void onFlushRequest();

signals:
	void audioDecodeError(const QString& errorMsg);

protected:
	void run() override;

private:
	void processFrame();
	void flushDecoder();
	void cleanup();

private:
	std::shared_ptr<MediaBuffer> buffer_;
	AVCodecContext* decCtx_;
	SwrContext* swrCtx_;
	AVFrame* decFrm_;
	AVFrame* pcmFrm_;
	std::atomic<bool> flush_;
	std::atomic<bool> running_;
};

