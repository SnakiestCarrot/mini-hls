
#pragma once
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <libavcodec/packet.h>

struct AVPacketDeleter {
    void operator ()(AVPacket* pkt) const { av_packet_free(&pkt); }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

class PacketQueue {
public:
    void push(AVPacketPtr pkt);
    AVPacketPtr pop();
    void close();

    PacketQueue(size_t max_queue_size) {
        max_size_ = max_queue_size;
        done_ = false;
    }

private:
    std::queue<AVPacketPtr> q_;
    std::mutex m_;
    std::condition_variable cv_;
    bool done_;
    size_t max_size_;
};

void PacketQueue::push(AVPacketPtr pkt) {
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [&]{ return q_.size() < max_size_ || done_; });
    q_.push(std::move(pkt));
    lock.unlock();
    cv_.notify_one();
}

AVPacketPtr PacketQueue::pop() {
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [&]{ return !q_.empty() || done_; });

    if (q_.empty()) {
        return nullptr;
    } 
    
    AVPacketPtr pkt = std::move(q_.front());
    q_.pop();
    lock.unlock();
    cv_.notify_one();
    return pkt;
}

void PacketQueue::close() {
    std::unique_lock<std::mutex> lock(m_);
    done_ = true;
    lock.unlock();
    cv_.notify_all();
}




