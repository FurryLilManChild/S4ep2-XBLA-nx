// S4ep2-XBLA-nx (2026-11-19): Episode METAL without Episode I.
//
// On the 360, Episode II unlocks Episode METAL when the player owns Episode I. The game's "Check Ep1 License" task
// (string 0x821059B8; constructor sub_823777A8, vtable 0x821059CC) runs a worker, sub_823779F8, that enumerates
// marketplace content (XContentCreateEnumerator, content type 0x000D0000), looks for title id 0x58410A07 (Episode I),
// mounts it as "epo" and reads its license; with the license bit set it writes 1 to [0x82AABC38]. That word is the
// game's "Episode I is licensed" flag: sub_82343CB8 starts the check only while it is 0, and the menus
// (sub_8232ED30 and others) read it. The constructor clears it to 0.
//
// s4_episode_metal (on by default) sets the flag to 1 before the game's start check (sub_82343CB8, which then starts no
// task) and after the worker runs (in case a task started anyway), and logs the first time.
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include <atomic>
#include <cstdint>
#include <cstring>

REXCVAR_DEFINE_BOOL(s4_episode_metal, true, "S4ep2-XBLA-nx",
                    "Episode METAL unlocked without Episode I (the game's Episode I license flag set; s4/s4_metal.cpp)");

REX_EXTERN(__imp__sub_82343CB8);
REX_EXTERN(__imp__sub_823779F8);

namespace {
constexpr uint32_t kEp1LicensedFlag = 0x82AABC38;  // S4ep2-XBLA-nx: written 1 by the worker sub_823779F8 (0x82377B18)

void SetFlag(uint8_t *base, const char *where) {
  if (!REXCVAR_GET(s4_episode_metal))
    return;
  const uint32_t one = __builtin_bswap32(1u);
  std::memcpy(base + kEp1LicensedFlag, &one, 4);
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true))
    REXLOG_INFO("[s4 metal] Episode I license flag set ({}): Episode METAL unlocked (s4_episode_metal)", where);
}
}  // namespace

// The start of the check: creates the task only while the flag is 0.
REX_HOOK_RAW(sub_82343CB8) {
  SetFlag(base, "before the license check");
  __imp__sub_82343CB8(ctx, base);
}

// The check's worker (a job; its address is passed by the constructor).
REX_HOOK_RAW(sub_823779F8) {
  __imp__sub_823779F8(ctx, base);
  SetFlag(base, "after the license check");
}
