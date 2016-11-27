#pragma once

template <typename T, uint8_t Size>
class RingBuffer {
 protected:
  T buf_[Size];
  uint8_t head_{0}, tail_{0};
 public:
  inline uint8_t nextPosition(uint8_t position) {
    return (position + 1) % Size;
  }

  inline uint8_t prevPosition(uint8_t position) {
    if (position == 0) {
      return Size - 1;
    }
    return position - 1;
  }

  inline bool enqueue(const T &item) {
    uint8_t next = nextPosition(head_);
    if (next == tail_) {
      // Full
      return false;
    }

    buf_[head_] = item;
    head_ = next;
    return true;
  }

  inline bool enqueue(const T *item, uint8_t num) {
    const T *end = item + num;
    uint8_t head = head_;

    while (item < end) {
      uint8_t next = nextPosition(head);
      if (next == tail_) {
        // No room for this many items
        return false;
      }
      buf_[head] = *item;
      head = next;
      ++item;
    }

    // We have enough room for all of it, so commit it
    head_ = head;
    return true;
  }

  inline uint8_t get(T *dest, uint8_t num, bool commit) {
    auto tail = tail_;
    uint8_t numFilled = 0;
    while (numFilled < num) {
      if (tail == head_) {
        // No more data
        break;
      }

      dest[numFilled++] = buf_[tail];
      tail = nextPosition(tail);
    }

    if (commit) {
      tail_ = tail;
    }
    return numFilled;
  }

  inline bool empty() const { return head_ == tail_; }
  inline uint8_t size() const {
    int diff = head_ - tail_;
    if (diff >= 0) {
      return diff;
    }
    return Size + diff;
  }

  inline T& front() {
    return buf_[tail_];
  }

  inline bool peek(T &item) {
    return get(&item, 1, false);
  }

  inline bool get(T &item) {
    return get(&item, 1, true);
  }
};
