// S4ep2-XBLA-nx (2026-11-19): the game's own render-target descriptors, checked, and the textures they lack made.
//
// Evidence (Mark's PC, build 850eb991):
//   - Texture dump: the only texture Renut knew of in the draws' pixel slots was 0x82996C4C, solid black.
//   - Resolve trace (banjo_renut_diag_resolve_trace, frames at 30 s and 60 s): the scene draws into a 1280x720
//     16-bit float target, then a chain of smaller targets (480x256 down to 1x1); EVERY one of those resolves has
//     destination texture 00000000. Only the Present's resolve of the back buffer has a texture. The passes after the
//     scene therefore sample nothing.
// The engine draws through descriptors:
//   - sub_828616E0 creates one (flags +0x00, target count +0x04, size +0x08/+0x0C, colour surfaces +0x14..+0x20,
//     depth surface +0x24, resolve textures +0x28..+0x34, depth texture +0x38, and at +0x5C a colour view of the
//     depth texture's memory). Its textures come from D3DXCreateTexture sub_82469398 (device r3, width r4, height r5,
//     levels r6, usage r7, format r8, pool r9, out pointer r10; its four callers are all in sub_828616E0), which
//     checks the request (sub_82468F68) and calls Direct3D's CreateTexture sub_823F26E8 (Renut's hook), storing the
//     texture it returns at the out pointer.
//   - sub_828620A8 switches to one; sub_828755A8 ends one, resolving into its textures through sub_823FD100.
// The textures the resolves name are 0, so D3DXCreateTexture fails or stores 0. This file logs each D3DXCreateTexture
// call (arguments, result, texture) and, when it leaves its out pointer 0 (s4_make_missing_textures, on by default),
// makes the texture with Direct3D's CreateTexture itself. It also logs the first descriptor switches and ends with
// the descriptor's words and which of an end's targets have no texture, and a count line every 10 s.
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "s4/s4_pacer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

REXCVAR_DEFINE_BOOL(s4_make_missing_textures, true, "S4ep2-XBLA-nx",
                    "When the game's D3DXCreateTexture (sub_82469398) leaves its texture pointer 0, make the texture "
                    "with Direct3D's CreateTexture (s4/s4_rt_diag.cpp)");

REX_EXTERN(__imp__sub_828620A8);
REX_EXTERN(__imp__sub_828755A8);
REX_EXTERN(__imp__sub_821EC0A0);
REX_EXTERN(__imp__sub_82469398);
REX_EXTERN(sub_823F26E8);

namespace {
constexpr uint32_t kCurrentDescriptor = 0x82AA33E8;     // S4ep2-XBLA-nx: [0x82AA33E8] the current descriptor
constexpr uint32_t kCurrentCopy = 0x82AA33EC;           // S4ep2-XBLA-nx: its copy, the end's default
constexpr uint32_t kBackBufferDescriptor = 0x82AA3358;  // S4ep2-XBLA-nx: built by sub_82861608
constexpr uint32_t kBegun = 0x82AABFCC;                 // S4ep2-XBLA-nx: set by the begin, cleared by the end
constexpr uint32_t kTiling = 0x82AA3428;                // S4ep2-XBLA-nx: tile list (0: the end's untiled path)
constexpr uint32_t kScreenA = 0x82996BF0;               // S4ep2-XBLA-nx: the descriptors sub_821EC0A0 samples
constexpr uint32_t kScreenB = 0x82997F68;               // S4ep2-XBLA-nx: (the other one)

uint32_t Load(const uint8_t *base, uint32_t va) {
  uint32_t v;
  std::memcpy(&v, base + va, 4);
  return __builtin_bswap32(v);
}

void Store(uint8_t *base, uint32_t va, uint32_t value) {
  const uint32_t v = __builtin_bswap32(value);
  std::memcpy(base + va, &v, 4);
}

std::atomic<uint32_t> g_switches{0}, g_ends{0}, g_ends_resolved{0}, g_resolves_by_game{0}, g_targets_no_texture{0},
    g_skip_not_begun{0}, g_skip_no_resolve_flag{0}, g_skip_no_mask{0}, g_skip_tiled{0}, g_composites{0},
    g_textures_asked{0}, g_textures_missing{0}, g_textures_made{0};

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
  REXLOG_INFO("[s4 rt] 10 s: descriptor switches {}, ends {} ({} made resolves, {} resolves), colour targets an end "
              "asked to resolve with no texture {}; ends with none: not begun {}, flag 0x20000 {}, tiled {}, no "
              "target bit {}; screen samples (sub_821EC0A0) {}; D3DXCreateTexture calls {}, left 0 {}, made by the "
              "port {} (s4_make_missing_textures {})",
              g_switches.exchange(0), g_ends.exchange(0), g_ends_resolved.exchange(0), g_resolves_by_game.exchange(0),
              g_targets_no_texture.exchange(0), g_skip_not_begun.exchange(0), g_skip_no_resolve_flag.exchange(0),
              g_skip_tiled.exchange(0), g_skip_no_mask.exchange(0), g_composites.exchange(0),
              g_textures_asked.exchange(0), g_textures_missing.exchange(0), g_textures_made.exchange(0),
              REXCVAR_GET(s4_make_missing_textures) ? "on" : "off");
}

void LogDescriptor(const char *what, const uint8_t *base, uint32_t d) {
  uint32_t w[15];
  for (int i = 0; i < 15; ++i) w[i] = d ? Load(base, d + 4 * i) : 0;
  REXLOG_INFO("[s4 rt] {} {:08X}: flags {:08X} count {} size {}x{} surfaces {:08X} {:08X} {:08X} {:08X} depth {:08X} "
              "textures {:08X} {:08X} {:08X} {:08X} depth texture {:08X}; current {:08X}, begun {}, tiles {:08X}",
              what, d, w[0], w[1], w[2], w[3], w[5], w[6], w[7], w[8], w[9], w[10], w[11], w[12], w[13], w[14],
              Load(base, kCurrentDescriptor), Load(base, kBegun), Load(base, kTiling));
}
}  // namespace

// D3DXCreateTexture: the descriptors' textures.
REX_HOOK_RAW(sub_82469398) {
  const uint32_t device = ctx.r3.u32, width = ctx.r4.u32, height = ctx.r5.u32, levels = ctx.r6.u32,
                 usage = ctx.r7.u32, format = ctx.r8.u32, pool = ctx.r9.u32, out = ctx.r10.u32;
  __imp__sub_82469398(ctx, base);
  const uint32_t result = ctx.r3.u32;
  const uint32_t texture = out ? Load(base, out) : 0;
  g_textures_asked.fetch_add(1, std::memory_order_relaxed);
  uint32_t made = 0;
  if (out && !texture) {
    g_textures_missing.fetch_add(1, std::memory_order_relaxed);
    if (REXCVAR_GET(s4_make_missing_textures)) {
      // Direct3D's CreateTexture as D3DXCreateTexture calls it (0x82469414..0x82469434): width r3, height r4, depth
      // 1 r5, levels r6, usage r7, format r8, pool r9, type 3 (texture) r10; it returns the texture.
      ctx.r3.u64 = width;
      ctx.r4.u64 = height;
      ctx.r5.u64 = 1;
      ctx.r6.u64 = levels ? levels : 1;
      ctx.r7.u64 = usage == 0xFFFFFFFFu ? 0 : usage;
      ctx.r8.u64 = format;
      ctx.r9.u64 = pool;
      ctx.r10.u64 = 3;
      sub_823F26E8(ctx, base);
      made = ctx.r3.u32;
      if (made) {
        Store(base, out, made);
        g_textures_made.fetch_add(1, std::memory_order_relaxed);
      }
      ctx.r3.u64 = made ? 0 : result;  // S_OK when made, else the game's own result
    }
  }
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1, std::memory_order_relaxed) < 80) {
    std::string note;
    if (made)
      note = fmt::format(", made by the port: {:08X}", made);
    else if (!out)
      note = ", no out pointer";
    else if (!texture)
      note = REXCVAR_GET(s4_make_missing_textures) ? ", CreateTexture made none either" : ", none (setting off)";
    REXLOG_INFO("[s4 rt] D3DXCreateTexture device {:08X} {}x{} levels {} usage {:08X} format {:08X} pool {} out "
                "{:08X} -> result {:08X}, texture {:08X}{}",
                device, width, height, levels, usage, format, pool, out, result, texture, note);
  }
}

// The switch: r3 the new descriptor (0: the back buffer's).
REX_HOOK_RAW(sub_828620A8) {
  static std::atomic<int> logged{0};
  g_switches.fetch_add(1, std::memory_order_relaxed);
  if (logged.fetch_add(1, std::memory_order_relaxed) < 60)
    LogDescriptor("switch to", base, ctx.r3.u32 ? ctx.r3.u32 : kBackBufferDescriptor);
  __imp__sub_828620A8(ctx, base);
  Report();
}

// The end: r3 the descriptor (0: the copy of the current one).
REX_HOOK_RAW(sub_828755A8) {
  const uint32_t d = ctx.r3.u32 ? ctx.r3.u32 : kCurrentCopy;
  const uint32_t flags = Load(base, d);
  const uint32_t count = Load(base, d + 4);
  const bool begun = Load(base, kBegun) != 0;
  const bool tiled = Load(base, kTiling) != 0;
  uint32_t asked = 0, missing = 0;
  for (uint32_t i = 0; i < count && i < 4; ++i) {
    if (flags & (1u << i)) {
      asked |= 1u << i;
      if (!Load(base, d + 40 + 4 * i))
        missing |= 1u << i;
    }
  }
  static std::atomic<int> logged{0};
  const bool log = logged.fetch_add(1, std::memory_order_relaxed) < 60;
  if (log)
    LogDescriptor("end", base, d);

  const uint32_t before = s4::g_resolve_calls.load(std::memory_order_relaxed);
  __imp__sub_828755A8(ctx, base);
  const uint32_t made = s4::g_resolve_calls.load(std::memory_order_relaxed) - before;

  g_ends.fetch_add(1, std::memory_order_relaxed);
  if (made) {
    g_ends_resolved.fetch_add(1, std::memory_order_relaxed);
    g_resolves_by_game.fetch_add(made, std::memory_order_relaxed);
    g_targets_no_texture.fetch_add(uint32_t(__builtin_popcount(missing)), std::memory_order_relaxed);
  } else if (!begun) {
    g_skip_not_begun.fetch_add(1, std::memory_order_relaxed);
  } else if (flags & 0x20000) {
    g_skip_no_resolve_flag.fetch_add(1, std::memory_order_relaxed);
  } else if (tiled) {
    g_skip_tiled.fetch_add(1, std::memory_order_relaxed);
  } else if (!asked) {
    g_skip_no_mask.fetch_add(1, std::memory_order_relaxed);
  }
  if (log)
    REXLOG_INFO("[s4 rt] end {:08X}: {} resolves made by the game (begun {}, tiled {}, target bits asked {:X}, of them "
                "with no texture {:X})",
                d, made, begun ? 1 : 0, tiled ? 1 : 0, asked, missing);
  Report();
}

// The screen sample: which descriptor's texture goes to the sampler (when [0x82AA96A4] names no other).
REX_HOOK_RAW(sub_821EC0A0) {
  static std::atomic<int> logged{0};
  g_composites.fetch_add(1, std::memory_order_relaxed);
  if (logged.fetch_add(1, std::memory_order_relaxed) < 8) {
    const uint32_t cur = Load(base, kCurrentDescriptor);
    const uint32_t d = cur == kScreenA ? kScreenB : kScreenA;
    REXLOG_INFO("[s4 rt] screen sample: current {:08X}; {:08X}'s texture {:08X} (unless the override names another)",
                cur, d, Load(base, d + 40));
  }
  __imp__sub_821EC0A0(ctx, base);
}
