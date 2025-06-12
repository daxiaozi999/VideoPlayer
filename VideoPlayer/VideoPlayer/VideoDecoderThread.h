#pragma once

#include <QObject>
#include <QThread>
#include <atomic>
#include <memory>
#include "SDK.h"
#include "MediaBuffer.h"
#include "MediaContext.h"

class VideoDecoderThread : public QThread {
	Q_OBJECT

public:
	VideoDecoderThread(QObject* parent = nullptr, std::shared_ptr<MediaBuffer> buffer = nullptr);
	~VideoDecoderThread() override;

	bool init();
	void start();
	void stop();

public slots:
	void onFlushRequest();

signals:
	void videoDecodeError(const QString& errorMsg);

protected:
	void run() override;

private:
	void processFrame();
	void flushDecoder();
	void cleanup();

private:
	std::shared_ptr<MediaBuffer> buffer_;
	AVCodecContext* decCtx_;
	SwsContext* swsCtx_;
	AVFrame* decFrm_;
	AVFrame* yuvFrm_;
	std::atomic<bool> flush_;
	std::atomic<bool> running_;
};

