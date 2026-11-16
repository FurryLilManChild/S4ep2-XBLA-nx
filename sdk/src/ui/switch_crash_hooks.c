/*
 * Silent deaths on Switch: abort(), exit() and whatever goes to stderr.
 *
 * newlib's abort() returns to the HOME menu without a word (measured on the first
 * boot of the SDK, see switch_libc_supplement.c). rex::FatalError ends there, as do
 * std::terminate (an uncaught exception, a std::thread destroyed without join) and
 * Mesa's aborts. And they all warn on stderr first, which goes nowhere in an NRO.
 *
 * It has to be in the executable, in rexglue_switch_startup, and not in a library:
 * the definition of abort has to reach the link before the one in libc.a to
 * replace it.
 *
 *   stderr  -> <NRO folder>/logs/rex/rex_stderr.log, unbuffered, rewritten on every boot
 *   abort() -> <NRO folder>/logs/rex/rex_crash.log with the stack, and error 2345-0101
 *   exit()  -> the same, error 2345-0103. The orderly shutdown does not go through exit()
 *              (see windowed_app_main_switch.cpp), so getting here means some library
 *              decided to end the process on its own.
 *
 * The rexglue logs go to <NRO folder>/logs/rex/ and not to sdmc:/switch/, so they do not mix with
 * those of other programs or with the game's, which go to <NRO folder>/logs/. The folder comes from
 * argv[0], which libnx fills in before the constructors. Without argv[0], sdmc:/switch/ is used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <switch.h>

/* Filled in by libnx (argv.c), which does not declare them in its headers. */
extern int __system_argc;
extern char** __system_argv;

/* Ends in '/'. Computed once at startup and never changes: the fault handler reads it without allocating memory. */
static char g_rex_log_dir[FS_MAX_PATH] = "sdmc:/switch/";

const char* RexSwitchLogDir(void) {
    return g_rex_log_dir;
}

static void RexSwitchInitLogDir(void) {
    if (__system_argc < 1 || !__system_argv || !__system_argv[0]) {
        return;
    }
    const char* nro = __system_argv[0];
    const char* bar = strrchr(nro, '/');
    if (!bar) {
        return;
    }
    /* without a device ("/switch/..."), the SD one: the path must also work for fsdevTranslatePath */
    const char* vulkan_device = strchr(nro, ':') ? "" : "sdmc:";
    char dir[FS_MAX_PATH];
    int n = snprintf(dir, sizeof(dir), "%s%.*slogs", vulkan_device, (int)(bar - nro + 1), nro);
    if (n <= 0 || (size_t)n + sizeof("/rex/") > sizeof(dir)) {
        return;
    }
    mkdir(dir, 0777);
    strcat(dir, "/rex");
    mkdir(dir, 0777);
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        strcat(dir, "/");
        strcpy(g_rex_log_dir, dir);
    }
}

void RexSwitchCrashLog(const char* reason, const ThreadExceptionDump* ctx, u64 stack, u64 pc);

/* TFU-nx (2026-11-12): the build's stamp (version, commit, build time), the first line of rex_stderr.log and of every
 * crash report, so each log is tied to its NRO. The app defines it (src/tfu_build_stamp.cpp); this is the fallback. */
__attribute__((weak)) const char* RexAppBuildStamp(void) {
    return "unknown build";
}

void abort(void) {
    RexSwitchCrashLog("abort()", NULL, (u64)__builtin_frame_address(0),
                      (u64)__builtin_return_address(0));
    diagAbortWithResult(MAKERESULT(Module_Libnx, 101));
}

static void RexSwitchExitHook(void) {
    RexSwitchCrashLog("exit() outside an orderly shutdown", NULL, (u64)__builtin_frame_address(0),
                      (u64)__builtin_return_address(0));
    diagAbortWithResult(MAKERESULT(Module_Libnx, 103));
}

/*
 * Priority 101, the first one allowed: before any static constructor of the
 * SDK, which can also die. The SD is already mounted: __appInit runs before the
 * constructors.
 */
__attribute__((constructor(101)))
static void RexSwitchCrashHooksInit(void) {
    RexSwitchInitLogDir();
    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%srex_stderr.log", g_rex_log_dir);
    /* TFU-nx (2026-11-12): the last launch's stderr is kept as rex_stderr.prev.log (a test often takes two launches,
     * and the second one erased the first's). A file that cannot be renamed is deleted. */
    char prev[FS_MAX_PATH];
    snprintf(prev, sizeof(prev), "%srex_stderr.prev.log", g_rex_log_dir);
    remove(prev);
    if (rename(path, prev) != 0) {
        remove(path);
    }
    if (freopen(path, "w", stderr)) {
        setvbuf(stderr, NULL, _IONBF, 0);
        fprintf(stderr, "%s\n", RexAppBuildStamp());
    }
    atexit(RexSwitchExitHook);
}
