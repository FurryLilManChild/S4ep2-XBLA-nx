// TFU-nx (2026-11-12): the build stamp (app/cmake/tfu_build_stamp.cmake writes tfu_build_stamp.h on every build).
// The Switch crash hooks print it first in rex_stderr.log and in every crash report; the app logs it at start.
#include "tfu_build_stamp.h"

extern "C" const char* RexAppBuildStamp(void) {
  return TFU_BUILD_STAMP;
}
