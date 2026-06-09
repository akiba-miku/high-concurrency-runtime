// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
// Reference:
//
#pragma once

#include <array>
#include <bitset>
#include <cstdint>

#include "runtime/base/noncopyable.h"

namespace runtime::ds {

template <typename T>
struct TimingNode {
  TimingNode<T>* next{nullptr};
  TimingNode<T>** pprev{nullptr};
  T* owner{nullptr};
  uint64_t expires{0};
  unsigned idx{0};
  bool pending{false};
};

template <typename T, TimingNode<T> T::*kMember>
class IntrusiveTimingWheel : public runtime::base::NonCopyable {
public:
  static constexpr unsigned kBitsPerLevel = 6;
  static constexpr unsigned kBucketsPerLevel = 1u << kBitsPerLevel;  // 64
  static constexpr unsigned kBucketsIndexMask = kBucketsPerLevel - 1;

  static constexpr unsigned kGranularityShiftPerLevel = 3;
  static constexpr unsigned kGranularityMultiplier = 1u << kGranularityShiftPerLevel;
  static constexpr unsigned kGranularityMask = kGranularityMultiplier - 1;

  static constexpr unsigned kLevelCount = 9;
  static constexpr unsigned kTotalBucketCount = kBucketsPerLevel * kLevelCount;  // 64 * 9 = 576
  static constexpr uint64_t kNextMaxDelta = 1ull << 30;

  static constexpr unsigned LvlShift(unsigned level) {
    return level * kGranularityShiftPerLevel;
  }
  static constexpr uint64_t LvlGran(unsigned level) { return 1ull << LvlShift(level); }
  static constexpr uint64_t LvlStart(unsigned level) {
    return static_cast<uint64_t>(kBucketsPerLevel - 1) << ((level - 1) * kGranularityShiftPerLevel);
  }

  static constexpr unsigned LvlOffs(unsigned level) { return level * kBucketsPerLevel; }
  static constexpr uint64_t kCutoff = LvlStart(kLevelCount);
  static constexpr uint64_t kTimeoutMax = kCutoff - LvlGran(kLevelCount - 1);

  explicit IntrusiveTimingWheel(uint64_t now_tick = 0) : clk_(now_tick) {}
  ~IntrusiveTimingWheel() = default;

  // O(1) - no bucket is populated.
  bool empty() const { return pending_.none(); }

  // O(1) - current wheel time in ticks.
  uint64_t now() const { return clk_; }

  void Schedule(T* elem, uint64_t delay);

  bool Cancel(T* elem);

  template <typename OnExpire>
  void Advance(uint64_t now, OnExpire&& on_expire);

  uint64_t TicksToNext() const;
private:
  static TimingNode<T>* NodeOf(T* elem) { return &(elem->*kMember); }
  static T* ElemOf(TimingNode<T>* node) { return node->owner; }

  static unsigned CalcIndex(uint64_t expires, unsigned level) {
    expires = (expires >> LvlShift(level)) + 1;
    return LvlOffs(level) + static_cast<unsigned>(expires & kBucketsIndexMask);
  }
  static unsigned CalcWheelIndex(uint64_t expires, uint64_t clk);

  void Enqueue(TimingNode<T>* node, unsigned idx);
  static void Detach(TimingNode<T>* node) {
    *(node->pprev) = node->next;
    if (node->next) *(node->next->pprev) = node->pprev;
    node->next = nullptr;
    node->pprev = nullptr;
    node->pending = false;
  }

  int Collect(uint64_t clk, std::array<TimingNode<T>*, kLevelCount>& heads);
  template <typename OnExpire>
  void Expire(TimingNode<T>*& head, OnExpire& on_expire);

  int NextPeningBucket(unsigned offset, unsigned clk) const;
  int FindNextBit(unsigned from, unsigned to) const {
    for (unsigned i = from; i < to; ++i) {
      if (pending_.test(i)) return static_cast<int>(i);
    }
    return -1;
  }

  std::array<TimingNode<T>*, kTotalBucketCount> vectors_{};
  std::bitset<kTotalBucketCount> pending_{};
  uint64_t clk_;
};

#define ITW_TMPL template <typename T, TimingNode<T> T::*kMember>
#define ITW_TYPE IntrusiveTimingWheel<T, kMember>

ITW_TMPL
void ITW_TYPE::Schedule(T* elem, uint64_t delay) {
  auto* node = NodeOf(elem);
  if (node->pending) {
    Cancel(elem);
  }
  node->owner = elem;
  node->expires = clk_ + delay;
  Enqueue(node, CalcWheelIndex(node->expires, clk_));
}

ITW_TMPL
bool ITW_TYPE::Cancel(T* elem) {
  auto* node = NodeOf(elem);
  if (!node->pending) return false;
  if (node->pprev == &vectors_[node->idx] && node->next == nullptr) {
    pending_.reset(node->idx);
  }
  Detach(node);
  return true;
}

ITW_TMPL
unsigned ITW_TYPE::CalcWheelIndex(uint64_t expires, uint64_t clk) {
  uint64_t delta = expires - clk;
  for (unsigned level = 0; level < kLevelCount - 1; ++level) {
    if (delta < LvlStart(level + 1)) return CalcIndex(expires, level);
  }
  if (static_cast<int64_t>(delta) < 0) {
    // Already expired: drop into the current level-0 bucket to fire next tick.
    return static_cast<unsigned>(clk & kBucketsIndexMask);
  }
  if (delta >= kCutoff) expires = clk + kTimeoutMax;
  return CalcIndex(expires, kLevelCount - 1);
}

ITW_TMPL
void ITW_TYPE::Enqueue(TimingNode<T>* node, unsigned idx) {
  TimingNode<T>*& head = vectors_[idx];
  node->next = head;
  if (head) head->pprev = &node->next;
  head = node;
  node->pprev = &vectors_[idx];
  node->idx = idx;
  node->pending = true;
  pending_.set(idx);
}

ITW_TMPL
template <typename OnExpire>
void ITW_TYPE::Advance(uint64_t now, OnExpire&& on_expire) {
  while ()
}
#undef ITW_TYPE
#undef ITW_TMPL
}  // namespace runtime::ds
