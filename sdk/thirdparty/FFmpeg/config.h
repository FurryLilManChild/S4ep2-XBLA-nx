/* TFU-nx: the platform's FFmpeg configuration, then the fork's WMV3 additions.
 * (rr6-nx kept this file outside git; written here so every build finds it.) */
#if defined(__SWITCH__)
#include "config_switch_aarch64.h"
#elif defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
#include "config_windows_aarch64.h"
#elif defined(_WIN32)
#include "config_windows_x86_64.h"
#elif defined(__APPLE__) && defined(__aarch64__)
#include "config_macos_aarch64.h"
#elif defined(__APPLE__)
#include "config_macos_x86_64.h"
#elif defined(__aarch64__)
#include "config_linux_aarch64.h"
#else
#include "config_linux_x86_64.h"
#endif
#include "config_masseffect_wmv3.h"
