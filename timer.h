#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

// Thread safe!
class Timer {
 private:
  std::chrono::high_resolution_clock::time_point time;
  std::mutex mmutex;

 public:
  Timer() { this->reset(); }

  void reset() {
    std::unique_lock<std::mutex> lck{mmutex};
    this->time = std::chrono::high_resolution_clock::now();
  }

  double getElapsedSeconds() {
    std::unique_lock<std::mutex> lck{mmutex};
    return 1.0e-6 * std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now() - this->time)
                        .count();
  }

  double getElapsedMilliseconds() {
    std::unique_lock<std::mutex> lck{mmutex};
    return 1.0e-3 * std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now() - this->time)
                        .count();
  }

  unsigned long long getElapsedMicroseconds() {
    std::unique_lock<std::mutex> lck{mmutex};
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::high_resolution_clock::now() - this->time)
        .count();
  }
};

// Similar to Timer, used to track expirations. Not thread safe.
class ExpirationTimer {
 public:
  using Duration = std::chrono::milliseconds;
  using Clock = std::chrono::steady_clock;
  using OnExpired = std::function<void()>;

  // Creates an ExpirationTimer.
  // @param name The name of the tiemr, useful for logging.
  // @param duration The amount of time before the timer expires.
  // @param on_expired A callback to invoke when expiration is detected.
  ExpirationTimer(const char* const name, const Duration& duration,
                  OnExpired&& on_expired = OnExpired())
      : name_(name), duration_(duration), on_expired_(std::move(on_expired)) {
    reset();
  }
  ExpirationTimer(ExpirationTimer&& o)
      : name_(std::move(o.name_)),
        duration_(std::move(o.duration_)),
        start_(std::move(o.start_)),
        on_expired_(std::move(o.on_expired_)) {}
  ExpirationTimer(const ExpirationTimer&) = delete;

  void reset() { start_ = Clock::now(); }

  // Amount of time left, clamped to zero.
  Duration expires_in() const {
    const auto time_left =
        duration_ - std::chrono::duration_cast<Duration>(Clock::now() - start_);
    return std::max(time_left, Duration::zero());
  }

  // Checks if the timer has expired. Calls the OnExpired callback and returns
  // true if so.
  bool handle_expiration() {
    auto now = Clock::now();
    if ((now - start_) > duration_) {
      if (on_expired_) {
        on_expired_();
      }
      start_ = now;
      return true;
    }

    return false;
  }

  const std::string& name() const { return name_; }

 private:
  std::string name_;
  const Duration duration_;
  Clock::time_point start_;
  std::function<void()> on_expired_;
};

// Holds a set of expiration timeers and provides helpers to determin the next
// expiration time.
class ExpirationSet {
 public:
  ExpirationSet(std::initializer_list<ExpirationTimer*> l) : timers_(l) {}

  // Determines the next expiration time. Returns a pair of [expiration_time,
  // timer].
  std::pair<ExpirationTimer::Duration, ExpirationTimer*> next_expiration()
      const {
    auto min = ExpirationTimer::Duration::max();
    ExpirationTimer* min_element = nullptr;
    for (auto timer : timers_) {
      auto curr_val = timer->expires_in();
      if (curr_val < min) {
        min = curr_val;
        min_element = timer;
      }
    }

    return {min, min_element};
  }

 private:
  const std::vector<ExpirationTimer*> timers_;
};

namespace mtimer {
typedef uint64_t time_ms_t;
time_ms_t ms_since_boot();
}  // namespace mtimer
