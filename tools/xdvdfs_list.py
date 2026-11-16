#!/usr/bin/env python3
"""TFU-nx: list an Xbox 360 disc image (XDVDFS) and copy chosen files out of it.

Written for TFU-nx. Reads only; never changes the image.

  python3 xdvdfs_list.py IMAGE.iso                     list every file with its size
  python3 xdvdfs_list.py IMAGE.iso --get default.xex OUT_DIR   copy one file (path as listed) into OUT_DIR
  python3 xdvdfs_list.py IMAGE.iso --extract-all OUT_DIR       copy every file (skips files already there)
"""
import os
import struct
import sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"
BASES = [0x00000000, 0x0FD90000, 0x02080000, 0x18300000]


def find_base(f):
    for base in BASES:
        f.seek(base + 32 * SECTOR)
        if f.read(20) == MAGIC:
            return base
    raise SystemExit("no XDVDFS volume found")


def walk(f, base, sector, size, prefix, out):
    f.seek(base + sector * SECTOR)
    data = f.read(size)
    stack = [0]
    seen = set()
    while stack:
        off = stack.pop()
        if off in seen or off * 4 + 14 > len(data):
            continue
        seen.add(off)
        p = off * 4
        left, right, start, length, attr, nlen = struct.unpack_from("<HHIIBB", data, p)
        if left == 0xFFFF and right == 0xFFFF and nlen in (0, 0xFF):
            continue
        name = data[p + 14:p + 14 + nlen].decode("latin-1")
        if left not in (0, 0xFFFF):
            stack.append(left)
        if right not in (0, 0xFFFF):
            stack.append(right)
        path = prefix + name
        if attr & 0x10:
            if length:
                walk(f, base, start, length, path + "/", out)
        else:
            out.append((path, start, length))


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    img = sys.argv[1]
    with open(img, "rb") as f:
        base = find_base(f)
        f.seek(base + 32 * SECTOR + 20)
        root_sector, root_size = struct.unpack("<II", f.read(8))
        files = []
        walk(f, base, root_sector, root_size, "", files)
        files.sort()
        if len(sys.argv) >= 4 and sys.argv[2] == "--extract-all":
            outdir = sys.argv[3]
            done = 0
            for path, start, length in files:
                dst = os.path.join(outdir, *path.split("/"))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                if os.path.exists(dst) and os.path.getsize(dst) == length:
                    done += 1
                    continue
                f.seek(base + start * SECTOR)
                with open(dst, "wb") as o:
                    left = length
                    while left:
                        chunk = f.read(min(left, 8 << 20))
                        o.write(chunk)
                        left -= len(chunk)
                done += 1
                if done % 100 == 0:
                    print("%d of %d files" % (done, len(files)), flush=True)
            print("extracted %d files into %s" % (done, outdir))
            return
        if len(sys.argv) >= 5 and sys.argv[2] == "--get":
            want, outdir = sys.argv[3], sys.argv[4]
            for path, start, length in files:
                if path.lower() == want.lower():
                    os.makedirs(outdir, exist_ok=True)
                    dst = os.path.join(outdir, os.path.basename(path))
                    f.seek(base + start * SECTOR)
                    with open(dst, "wb") as o:
                        left = length
                        while left:
                            chunk = f.read(min(left, 8 << 20))
                            o.write(chunk)
                            left -= len(chunk)
                    print("wrote", dst, length)
                    return
            raise SystemExit("not found: " + want)
        total = 0
        print("partition base 0x%08X" % base)
        for path, start, length in files:
            print("%12d  %s" % (length, path))
            total += length
        print("%d files, %.2f GB" % (len(files), total / 1e9))


if __name__ == "__main__":
    main()
