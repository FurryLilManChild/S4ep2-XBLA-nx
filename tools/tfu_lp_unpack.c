/* TFU-nx (2026-11-11): unpacks one of The Force Unleashed's level packs (LevelPacks/*.lp) for the offline shader
 * dump (tools/tfu_pack_shaders.py). A host tool, not shipped; it writes the pack's data, which is game data: keep
 * its output out of git (out/ is ignored).
 *
 * The pack, as read from the files (no documentation): a 16-byte header (0x0FF512ED, 0x01000000, two words that look
 * like a hash), a table of big-endian section sizes (uncompressed: they add up to about 2.5 times the file), then the
 * sections, each an XMemCompress LZX stream of its own (window 2^17, checked on dea_throne_x360.lp: all 19 sections
 * of its first 2.4 MB decode to their table sizes).
 *
 *   tfu_lp_unpack <pack.lp> <out.bin> [table entries] [window bits]
 * Without the table entry count, the table is read up to the first word that is no plausible size (over 16 MB or 0).
 * Build: gcc -O2 -I(sdk)/thirdparty/libmspack/cabextract/mspack tools/tfu_lp_unpack.c
 *        <same>/lzxd.c <same>/system.c -o out/tools/tfu_lp_unpack  (same = the mspack folder)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzx.h"
#include "system.h"

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint32_t be16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: tfu_lp_unpack <pack.lp> <out.bin> [table entries] [window bits]\n");
    return 2;
  }
  FILE *f = fopen(argv[1], "rb");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", argv[1]);
    return 1;
  }
  fseek(f, 0, SEEK_END);
  const long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *data = malloc((size_t)size);
  if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) {
    fprintf(stderr, "cannot read %s\n", argv[1]);
    return 1;
  }
  fclose(f);
  if (size < 32 || be32(data) != 0x0FF512EDu) {
    fprintf(stderr, "%s: not a 0x0FF512ED pack\n", argv[1]);
    return 1;
  }
  long entries = argc > 3 ? strtol(argv[3], NULL, 0) : 0;
  if (entries <= 0) {
    entries = 0;
    while (16 + 4 * (entries + 1) <= size) {
      const uint32_t v = be32(data + 16 + 4 * entries);
      if (v == 0 || v > 0x1000000u)
        break;
      ++entries;
    }
  }
  uint64_t total = 0;
  for (long i = 0; i < entries; ++i)
    total += be32(data + 16 + 4 * i);
  long pos = 16 + 4 * entries;

  /* Each section is its own LZX stream (window 2^17): frames of 32 KB of output, each after its 2-byte big-endian
   * compressed size (or 0xFF, 2-byte output size, 2-byte compressed size), the last one shorter. A section starts
   * right after the one before, or at the next 64 KB of the file with zeros between (dea_throne: 0x25FFFE ->
   * 0x260000 after 2 zero bytes, 0x30FFFFF -> 0x3100000 after 1, 0x79FFEE after 18): when a section does not decode
   * where the last one ended, it is tried at the next 64 KB. The sections are written one after the other. */
  const int bits = argc > 4 ? atoi(argv[4]) : 17;
  FILE *out_file = fopen(argv[2], "wb");
  uint8_t *joined = malloc(0x1000000);
  if (!out_file || !joined)
    return 1;
  const char *tmp_in = "tfu_lp_unpack.in.tmp", *tmp_out = "tfu_lp_unpack.out.tmp";
  uint64_t written = 0;
  long failed = 0, realigned = 0;
  for (long section = 0; section < entries; ++section) {
    const uint32_t out_len = be32(data + 16 + 4 * section);
    const uint32_t frames = (out_len + 0x7FFF) / 0x8000;
    int err = -1;
    long end = pos;
    for (int attempt = 0; attempt < 2 && err; ++attempt) {
      long at = attempt ? ((pos + 0xFFFF) & ~0xFFFFL) : pos;
      if (attempt && at == pos)
        break;
      size_t joined_len = 0;
      int ok = 1;
      for (uint32_t frame = 0; frame < frames; ++frame) {
        if (at + 2 > size) {
          ok = 0;
          break;
        }
        uint32_t in_len;
        if (data[at] == 0xFF) {
          in_len = be16(data + at + 3);
          at += 5;
        } else {
          in_len = be16(data + at);
          at += 2;
        }
        if (in_len == 0 || at + in_len > size || joined_len + in_len > 0x1000000) {
          ok = 0;
          break;
        }
        memcpy(joined + joined_len, data + at, in_len);
        joined_len += in_len;
        at += in_len;
      }
      if (!ok)
        continue;
      FILE *t = fopen(tmp_in, "wb");
      fwrite(joined, 1, joined_len, t);
      fclose(t);
      struct mspack_system *sys = mspack_default_system;
      struct mspack_file *in = sys->open(sys, tmp_in, MSPACK_SYS_OPEN_READ);
      struct mspack_file *out = sys->open(sys, tmp_out, MSPACK_SYS_OPEN_WRITE);
      struct lzxd_stream *lzx = lzxd_init(sys, in, out, bits, 0, 0x10000, (off_t)out_len, 0);
      err = lzx ? lzxd_decompress(lzx, (off_t)out_len) : -1;
      if (lzx)
        lzxd_free(lzx);
      sys->close(in);
      sys->close(out);
      if (!err) {
        end = at;
        realigned += attempt;
      }
    }
    if (err) {
      ++failed;
      fprintf(stderr, "section %ld (%u bytes) at %ld: not decoded; the rest of the pack is skipped\n", section,
              out_len, pos);
      break;
    }
    pos = end;
    FILE *o = fopen(tmp_out, "rb");
    size_t got;
    static uint8_t buf[1 << 16];
    while ((got = fread(buf, 1, sizeof(buf), o)) > 0)
      fwrite(buf, 1, got, out_file);
    fclose(o);
    written += out_len;
  }
  fclose(out_file);
  remove(tmp_in);
  remove(tmp_out);
  fprintf(stderr, "%s: %ld sections, %llu bytes written (%ld at a 64 KB boundary), %ld failed, %ld of %ld bytes read\n",
          argv[1], entries, (unsigned long long)written, realigned, failed, pos, size);
  printf("%llu\n", (unsigned long long)written);
  return failed ? 1 : 0;
}
