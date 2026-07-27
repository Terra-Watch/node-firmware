#include "PacketQueue.h"
namespace jalari::queue
{
    bool PacketQueue::enqueue(const protocol::Packet &packet) { if (full()) return false; packets_[tail_] = packet; tail_ = (tail_ + 1U) % packets_.size(); ++size_; return true; }
    bool PacketQueue::dequeue(protocol::Packet &outPacket) { if (empty()) return false; outPacket = packets_[head_]; head_ = (head_ + 1U) % packets_.size(); --size_; return true; }
    const protocol::Packet *PacketQueue::peek() const { return empty() ? nullptr : &packets_[head_]; }
    bool PacketQueue::empty() const { return size_ == 0U; }
    bool PacketQueue::full() const { return size_ == packets_.size(); }
    std::size_t PacketQueue::size() const { return size_; }
    void PacketQueue::clear() { head_ = 0; tail_ = 0; size_ = 0; }
} // namespace jalari::queue
