// S4ep2-XBLA-nx (2026-11-19): the game's own render-target descriptors, checked and (s4_force_descriptor_resolve)
// completed.
//
// What the texture dump showed (Mark's PC, build 850eb991): in every frame the only texture any draw sampled was
// 0x82996C4C, 1280x720, and it was solid black (every texel 000000FF), while the game issued 0 resolves. That header
// sits inside the render-target descriptor 0x82996BF0 (+0x5C). The engine draws through descriptors:
//   - sub_828616E0 creates one (flags +0x00, target count +0x04, size +0x08/+0x0C, colour surfaces +0x14..+0x20,
//     depth surface +0x24, resolve textures +0x28..+0x34, depth texture +0x38).
//   - sub_828620A8 switches to one: it ends the current one (sub_828755A8), copies the new one to 0x82AA33EC, makes it
//     current at [0x82AA33E8], binds its surfaces (sub_823F0260) and begins it (sub_828758F8).
//   - sub_828755A8 ends one: unless [0x82AABFCC] (set by the begin) is 0 or flag 0x20000 is set, it resolves each
//     colour target whose bit (1 << i) is in the flags into its texture, through Direct3D's Resolve sub_823FD100.
//   - sub_821EC0A0 samples the texture of 0x82996BF0, or of 0x82997F68 while 0x82996BF0 is current.
// So the black texture is what the game samples after drawing into 0x82996BF0, and it is black because nothing was
// resolved into it. This file logs each switch and each end with the descriptor's words, how many resolves the end
// made, and which of its conditions stopped it; and when an end makes none that its descriptor asks for, it makes them
// (s4_force_descriptor_resolve, on by default, counted in the 10 s line).
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "s4/s4_pacer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>

REXCVAR_DEFINE_BOOL(s4_force_descriptor_resolve, true, "S4ep2-XBLA-nx",
                    "When the game ends one of its render-target descriptors (sub_828755A8) without a resolve that "
                    "the descriptor asks for, make the resolve (s4/s4_rt_diag.cpp)");

REX_EXTERN(__imp__sub_828620A8);
REX_EXTERN(__imp__sub_828755A8);
REX_EXTERN(__imp__sub_821EC0A0);
REX_EXTERN(sub_823FD100);

namespace {
constexpr uint32_t kCurrentDescriptor = 0x82AA33E8;   // S4ep2-XBLA-nx: [0x82AA33E8] the current descriptor
constexpr uint32_t kCurrentCopy = 0x82AA33EC;         // S4ep2-XBLA-nx: its copy, the end's default
constexpr uint32_t kBackBufferDescriptor = 0x82AA3358;  // S4ep2-XBLA-nx: built by sub_82861608
constexpr uint32_t kBegun = 0x82AABFCC;               // S4ep2-XBLA-nx: set by the begin, cleared by the end
constexpr uint32_t kTiling = 0x82AA3428;              // S4ep2-XBLA-nx: tile list (0: the end's untiled path)
constexpr uint32_t kDevice = 0x82AAC01C;              // S4ep2-XBLA-nx: [0x82AAC01C] the device
constexpr uint32_t kScreenA = 0x82996BF0;             // S4ep2-XBLA-nx: the descriptors sub_821EC0A0 samples
constexpr uint32_t kScreenB = 0x82997F68;             // S4ep2-XBLA-nx: (the other one)

uint32_t Load(const uint8_t *base, uint32_t va) {
  uint32_t v;
  std::memcpy(&v, base + va, 4);
  return __builtin_bswap32(v);
}

std::atomic<uint32_t> g_switches{0}, g_ends{0}, g_ends_resolved{0}, g_resolves_by_game{0}, g_forced{0},
    g_skip_not_begun{0}, g_skip_no_resolve_flag{0}, g_skip_no_mask{0}, g_skip_back_buffer{0}, g_composites{0};

void Report() {
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
  REXLOG_INFO("[s4 rt] 10 s: descriptor switches {}, ends {} ({} made resolves, {} resolves), forced resolves {} "
              "(s4_force_descriptor_resolve {}); ends with none: not begun {}, flag 0x20000 {}, no target bit {}, "
              "back buffer {}; screen samples (sub_821EC0A0) {}",
              g_switches.exchange(0), g_ends.exchange(0), g_ends_resolved.exchange(0), g_resolves_by_game.exchange(0),
              g_forced.exchange(0), REXCVAR_GET(s4_force_descriptor_resolve) ? "on" : "off",
              g_skip_not_begun.exchange(0), g_skip_no_resolve_flag.exchange(0), g_skip_no_mask.exchange(0),
              g_skip_back_buffer.exchange(0), g_composites.exchange(0));
}

void LogDescriptor(const char *what, const uint8_t *base, uint32_t d, uint32_t lr, uint32_t extra) {
  uint32_t w[15];
  for (int i = 0; i < 15; ++i) w[i] = d ? Load(base, d + 4 * i) : 0;
  REXLOG_INFO("[s4 rt] {} {:08X} from {:08X} ({:08X}): flags {:08X} count {} size {}x{} surfaces {:08X} {:08X} "
              "{:08X} {:08X} depth {:08X} textures {:08X} {:08X} {:08X} {:08X} depth texture {:08X}; current {:08X}, "
              "begun {}, tiles {:08X}",
              what, d, lr, extra, w[0], w[1], w[2], w[3], w[5], w[6], w[7], w[8], w[9], w[10], w[11], w[12], w[13],
              w[14], Load(base, kCurrentDescriptor), Load(base, kBegun), Load(base, kTiling));
}
}  // namespace

// The switch: r3 the new descriptor (0: the back buffer's).
REX_HOOK_RAW(sub_828620A8) {
  static std::atomic<int> logged{0};
  g_switches.fetch_add(1, std::memory_order_relaxed);
  if (logged.fetch_add(1, std::memory_order_relaxed) < 60) {
    const uint32_t d = ctx.r3.u32 ? ctx.r3.u32 : kBackBufferDescriptor;
    LogDescriptor("switch to", base, d, uint32_t(ctx.lr), ctx.r4.u32);
  }
  __imp__sub_828620A8(ctx, base);
  Report();
}

// The end: r3 the descriptor (0: the copy of the current one).
REX_HOOK_RAW(sub_828755A8) {
  const uint32_t d = ctx.r3.u32 ? ctx.r3.u32 : kCurrentCopy;
  const uint32_t lr = uint32_t(ctx.lr);
  const uint32_t flags = Load(base, d);
  const uint32_t count = Load(base, d + 4);
  const bool begun = Load(base, kBegun) != 0;
  const bool tiled = Load(base, kTiling) != 0;
  uint32_t mask = 0;
  for (uint32_t i = 0; i < count && i < 4; ++i)
    if ((flags & (1u << i)) && Load(base, d + 40 + 4 * i))
      mask |= 1u << i;
  static std::atomic<int> logged{0};
  const bool log = logged.fetch_add(1, std::memory_order_relaxed) < 60;
  if (log)
    LogDescriptor("end", base, d, lr, ctx.r3.u32);

  const uint32_t before = s4::g_resolve_calls.load(std::memory_order_relaxed);
  __imp__sub_828755A8(ctx, base);
  const uint32_t made = s4::g_resolve_calls.load(std::memory_order_relaxed) - before;

  g_ends.fetch_add(1, std::memory_order_relaxed);
  if (made) {
    g_ends_resolved.fetch_add(1, std::memory_order_relaxed);
    g_resolves_by_game.fetch_add(made, std::memory_order_relaxed);
  } else if (!begun) {
    g_skip_not_begun.fetch_add(1, std::memory_order_relaxed);
  } else if (flags & 0x20000) {
    g_skip_no_resolve_flag.fetch_add(1, std::memory_order_relaxed);
  } else if (!mask) {
    g_skip_no_mask.fetch_add(1, std::memory_order_relaxed);
  }
  const bool back_buffer = Load(base, d + 0x14) == Load(base, kBackBufferDescriptor + 0x14) && !mask;
  if (back_buffer && !made)
    g_skip_back_buffer.fetch_add(1, std::memory_order_relaxed);

  uint32_t forced = 0;
  if (!made && mask && !tiled && REXCVAR_GET(s4_force_descriptor_resolve)) {
    // The untiled end's own call (0x828758A8..0x828758C8): device, flags = the target index, the whole surface, the
    // texture, point (0, 0), no clear.
    const uint64_t r3 = ctx.r3.u64;
    for (uint32_t i = 0; i < 4; ++i) {
      if (!(mask & (1u << i)))
        continue;
      ctx.r3.u64 = Load(base, kDevice);
      ctx.r4.u64 = i;
      ctx.r5.u64 = 0;
      ctx.r6.u64 = Load(base, d + 40 + 4 * i);
      ctx.r7.u64 = 0;
      ctx.r8.u64 = 0;
      ctx.r9.u64 = 0;
      ctx.r10.u64 = 0;
      ctx.f1.f64 = 1.0;
      sub_823FD100(ctx, base);
      ++forced;
    }
    ctx.r3.u64 = r3;
    g_forced.fetch_add(forced, std::memory_order_relaxed);
  }
  if (log)
    REXLOG_INFO("[s4 rt] end {:08X}: {} resolves made by the game, {} forced (begun {}, tiled {}, target bits {:X})",
                d, made, forced, begun ? 1 : 0, tiled ? 1 : 0, mask);
  Report();
}

// The screen sample: which descriptor's texture goes to the sampler.
REX_HOOK_RAW(sub_821EC0A0) {
  static std::atomic<int> logged{0};
  g_composites.fetch_add(1, std::memory_order_relaxed);
  if (logged.fetch_add(1, std::memory_order_relaxed) < 8) {
    const uint32_t cur = Load(base, kCurrentDescriptor);
    const uint32_t d = cur == kScreenA ? kScreenB : kScreenA;
    REXLOG_INFO("[s4 rt] screen sample from {:08X}: current {:08X}, samples {:08X}'s texture {:08X}", uint32_t(ctx.lr),
                cur, d, Load(base, d + 40));
  }
  __imp__sub_821EC0A0(ctx, base);
}
