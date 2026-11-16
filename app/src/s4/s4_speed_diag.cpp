// S4ep1-nx (2026-11-15): the game-speed line. The frame pacer's per-frame step (sub_82488958) is counted here, for the
// pacer fix in the Present hook (s4::g_pacer_entries, gpu/d3d/hooks/draw.cpp) and for one log line every 10 s of real
// time: 600 pacer steps in 10 s is full speed. Trimmed for 0.5.0 from the slow-motion investigation's counters (the
// main loop, deadline waits, D3D interrupts and swap-done callbacks; docs/progress-log.md, 2026-11-15).
#include <rex/hook.h>
#include <rex/logging.h>

#include "s4/s4_pacer.h"

#include <atomic>
#include <chrono>
#include <cstdint>

REX_EXTERN(__imp__sub_82488958);

namespace {
std::atomic<uint32_t> g_pacer{0};

void MaybeReport() {
  static std::atomic<int64_t> next{0};
  const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
  int64_t due = next.load(std::memory_order_relaxed);
  if (due == 0) {
    next.compare_exchange_strong(due, now + 10000);
    return;
  }
  if (now < due || !next.compare_exchange_strong(due, now + 10000))
    return;
  REXLOG_INFO("[s4 speed] 10 s: pacer steps {} (600 = full speed); Presents that woke the frame thread {}, held back {}",
              g_pacer.exchange(0), s4::g_present_wakes.exchange(0), s4::g_present_skips.exchange(0));
}
}  // namespace

REX_HOOK_RAW(sub_82488958) {
  g_pacer.fetch_add(1, std::memory_order_relaxed);
  s4::g_pacer_entries.fetch_add(1, std::memory_order_release);
  MaybeReport();
  __imp__sub_82488958(ctx, base);
}
