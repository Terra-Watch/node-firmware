#include "PacketQueue.h"
namespace terrawatch::queue
{
    void PacketQueue::lock_() const
    {
#if defined(ARDUINO)
        portENTER_CRITICAL(&mutex_);
#endif
    }
    void PacketQueue::unlock_() const
    {
#if defined(ARDUINO)
        portEXIT_CRITICAL(&mutex_);
#endif
    }
    bool PacketQueue::enqueue(const protocol::Packet &packet)
    {
        lock_();
        if (size_ == packets_.size()) { unlock_(); return false; }
        packets_[tail_] = packet;
        tail_ = (tail_ + 1U) % packets_.size();
        ++size_;
        unlock_();
        return true;
    }
    bool PacketQueue::dequeue(protocol::Packet &outPacket)
    {
        lock_();
        if (size_ == 0U) { unlock_(); return false; }
        outPacket = packets_[head_];
        head_ = (head_ + 1U) % packets_.size();
        --size_;
        unlock_();
        return true;
    }
    const protocol::Packet *PacketQueue::peek() const
    {
        lock_();
        const protocol::Packet *packet = size_ == 0U ? nullptr : &packets_[head_];
        unlock_();
        return packet;
    }
    bool PacketQueue::empty() const { lock_(); const bool result = size_ == 0U; unlock_(); return result; }
    bool PacketQueue::full() const { lock_(); const bool result = size_ == packets_.size(); unlock_(); return result; }
    std::size_t PacketQueue::size() const { lock_(); const std::size_t result = size_; unlock_(); return result; }
    void PacketQueue::clear() { lock_(); head_ = 0; tail_ = 0; size_ = 0; unlock_(); }
} // namespace terrawatch::queue
