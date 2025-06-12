#pragma once
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <functional>

namespace queue {

    template<typename T>
    class Queue {
    public:
        using ClearCallback = std::function<void(T*)>;
        static constexpr size_t DEFAULT_QUEUE_SIZE = 5;

        Queue(size_t minSize, size_t maxSize)
            : clearCallback_(nullptr)
            , abort_(false)
            , minSize_(minSize <= 0 ? DEFAULT_QUEUE_SIZE : minSize)
            , maxSize_(maxSize < minSize ? minSize : maxSize) {
        }

        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;

        Queue(Queue&& other) noexcept
            : queue_(std::move(other.queue_))
            , clearCallback_(std::move(other.clearCallback_))
            , abort_(other.abort_.load())
            , minSize_(other.minSize_)
            , maxSize_(other.maxSize_) {
            other.abort_.store(true);
        }

        Queue& operator=(Queue&& other) noexcept {
            if (this != &other) {
                clear();
                queue_ = std::move(other.queue_);
                clearCallback_ = std::move(other.clearCallback_);
                abort_ = other.abort_.load();
                minSize_ = other.minSize_;
                maxSize_ = other.maxSize_;
                other.abort_.store(true);
            }
            return *this;
        }

        ~Queue() {
            abort();
            clear();
        }

        void setClearCallback(ClearCallback callback) {
            QMutexLocker locker(&mtx_);
            clearCallback_ = std::move(callback);
        }

        void setLimit(size_t minSize, size_t maxSize) {
            if (minSize <= 0 || maxSize <= 0 || minSize > maxSize) {
                return;
            }
            QMutexLocker locker(&mtx_);
            minSize_ = minSize;
            maxSize_ = maxSize;
        }

        bool enqueue(T* item) {
            if (!item || abort_.load()) {
                return false;
            }

            while (!abort_.load()) {
                {
                    QMutexLocker locker(&mtx_);
                    if (queue_.size() < maxSize_) {
                        queue_.enqueue(item);
                        notEmpty_.wakeOne();
                        return true;
                    }
                }

                QMutexLocker locker(&mtx_);
                if (!abort_.load()) {
                    notFull_.wait(&mtx_, 100);
                }
            }
            return false;
        }

        T* dequeue() {
            if (abort_.load()) {
                return nullptr;
            }

            while (!abort_.load()) {
                {
                    QMutexLocker locker(&mtx_);
                    if (!queue_.isEmpty()) {
                        T* item = queue_.dequeue();
                        if (queue_.size() <= minSize_) {
                            notFull_.wakeAll();
                        }
                        return item;
                    }
                }

                QMutexLocker locker(&mtx_);
                if (!abort_.load()) {
                    notEmpty_.wait(&mtx_, 100);
                }
            }
            return nullptr;
        }

        size_t size() const {
            QMutexLocker locker(&mtx_);
            return queue_.size();
        }

        bool empty() const {
            QMutexLocker locker(&mtx_);
            return queue_.isEmpty();
        }

        bool full() const {
            QMutexLocker locker(&mtx_);
            return queue_.size() >= maxSize_;
        }

        void wakeAll() {
            notFull_.wakeAll();
            notEmpty_.wakeAll();
        }

        void abort() {
            abort_.store(true);
            wakeAll();
        }

        void resume() {
            abort_.store(false);
        }

        void clear() {
            QMutexLocker locker(&mtx_);
            while (!queue_.isEmpty()) {
                T* item = queue_.dequeue();
                if (clearCallback_) {
                    clearCallback_(item);
                }
                else {
                    delete item;
                }
            }
        }

    private:
        mutable QMutex mtx_;
        QQueue<T*> queue_;
        QWaitCondition notEmpty_;
        QWaitCondition notFull_;
        ClearCallback clearCallback_;
        std::atomic<bool> abort_;
        size_t minSize_;
        size_t maxSize_;
    };

} // namespace queue