/* S4ep1-nx: decompress a dechunked XEX LZX stream with libmspack (from the SDK's thirdparty). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mspack.h"
#include "lzx.h"
typedef struct { struct mspack_system s; } msys;
typedef struct { unsigned char *b; off_t n, o; } mfile;
static int mread(struct mspack_file *f, void *buf, int c) { mfile *m = (mfile *)f; off_t r = m->n - m->o; if (r > c) r = c; memcpy(buf, m->b + m->o, r); m->o += r; return (int)r; }
static int mwrite(struct mspack_file *f, void *buf, int c) { mfile *m = (mfile *)f; off_t r = m->n - m->o; if (r > c) r = c; memcpy(m->b + m->o, buf, r); m->o += r; return (int)r; }
static void *malloc_(struct mspack_system *s, size_t n) { (void)s; return calloc(n, 1); }
static void free_(void *p) { free(p); }
static void copy_(void *a, void *b, size_t n) { memmove(b, a, n); }
int main(int argc, char **argv) {
  if (argc != 5) { fprintf(stderr, "usage: lzxd_mem <in> <out> <window_hex> <out_size_hex>\n"); return 2; }
  FILE *fi = fopen(argv[1], "rb"); fseek(fi, 0, SEEK_END); long n = ftell(fi); fseek(fi, 0, SEEK_SET);
  unsigned char *in = malloc(n); fread(in, 1, n, fi); fclose(fi);
  unsigned w = strtoul(argv[3], 0, 16), osz = strtoul(argv[4], 0, 16), bits = 0; while ((1u << bits) < w) bits++;
  unsigned char *out = calloc(osz, 1);
  struct mspack_system sys = {0}; sys.read = mread; sys.write = mwrite; sys.alloc = malloc_; sys.free = free_; sys.copy = copy_;
  mfile src = {in, n, 0}, dst = {out, osz, 0};
  struct lzxd_stream *z = lzxd_init(&sys, (struct mspack_file *)&src, (struct mspack_file *)&dst, bits, 0, 0x8000, osz, 0);
  int r = z ? lzxd_decompress(z, osz) : -1;
  FILE *fo = fopen(argv[2], "wb"); fwrite(out, 1, osz, fo); fclose(fo);
  printf("lzxd result %d, wrote 0x%X bytes (window bits %u)\n", r, osz, bits); return r != 0;
}
