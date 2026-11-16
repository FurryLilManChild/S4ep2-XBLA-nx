// S4ep1-nx (2026-11-15): shared between the speed check (s4/s4_speed_diag.cpp) and Renut's Present hook
// (renut_engine/gpu/d3d/hooks/draw.cpp).
#pragma once
#include <atomic>
#include <cstdint>

namespace s4 {
// Entries into the frame pacer (sub_82488958), counted from boot; never reset.
inline std::atomic<uint32_t> g_pacer_entries{0};
// Presents that woke the frame thread (the game's own path), and those that did not because a deadline step had
// already run since the previous Present (s4_pacer_fix), counted per 10 s window.
inline std::atomic<uint32_t> g_present_wakes{0}, g_present_skips{0};
}  // namespace s4
