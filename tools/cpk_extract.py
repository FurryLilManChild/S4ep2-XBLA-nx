#!/usr/bin/env python3
"""S4ep1-nx (2026-11-18): a small CRI CPK reader (the @UTF tables, CRILAYLA decompression), written for Sonic 4's
data.cpk. Lists the files or extracts them; tools/s4_cpk_shaders.py imports read_files() to scan them in memory.

  python3 -I tools/cpk_extract.py <file.cpk> list
  python3 -I tools/cpk_extract.py <file.cpk> extract <out folder> [name filter]
"""
import os
import struct
import sys


def read_utf(buf):
    # buf starts at '@UTF'
    assert buf[:4] == b'@UTF', buf[:4]
    size = struct.unpack('>I', buf[4:8])[0]
    t = buf[8:8 + size]
    rows_off = struct.unpack('>H', t[2:4])[0]
    strings_off, data_off, name_off = struct.unpack('>III', t[4:16])
    ncols, row_len, nrows = struct.unpack('>HHI', t[16:24])

    def cstr(off):
        s = t[strings_off + off:]
        return s[:s.index(b'\0')].decode('latin-1')

    cols = []
    p = 24
    for _ in range(ncols):
        flags = t[p]; p += 1
        name = cstr(struct.unpack('>I', t[p:p + 4])[0]); p += 4
        storage = flags & 0xF0; typ = flags & 0x0F
        const = None
        if storage == 0x30:  # constant
            const, p = read_val(t, p, typ, cstr, data_off)
        cols.append((name, storage, typ, const))
    rows = []
    for r in range(nrows):
        p = rows_off + r * row_len
        row = {}
        for name, storage, typ, const in cols:
            if storage == 0x50:
                row[name], p = read_val(t, p, typ, cstr, data_off)
            elif storage == 0x30:
                row[name] = const
            else:
                row[name] = None
        rows.append(row)
    return rows


def read_val(t, p, typ, cstr, data_off):
    if typ in (0, 1): return t[p], p + 1
    if typ in (2, 3): return struct.unpack('>H', t[p:p + 2])[0], p + 2
    if typ in (4, 5): return struct.unpack('>I', t[p:p + 4])[0], p + 4
    if typ in (6, 7): return struct.unpack('>Q', t[p:p + 8])[0], p + 8
    if typ == 8: return struct.unpack('>f', t[p:p + 4])[0], p + 4
    if typ == 0xA: return cstr(struct.unpack('>I', t[p:p + 4])[0]), p + 4
    if typ == 0xB:
        off, ln = struct.unpack('>II', t[p:p + 8])
        return t[data_off + off:data_off + off + ln], p + 8
    raise ValueError('type %x' % typ)


def utf_at(f, off):
    f.seek(off)
    hdr = f.read(16)
    # chunk header: 4-byte id, 4 unk, 8-byte size; @UTF follows (possibly encrypted; CPKs from 2010 usually not)
    size = struct.unpack('<Q', hdr[8:16])[0]
    data = f.read(size)
    if data[:4] != b'@UTF':
        data = decrypt(data)
    return read_utf(data)


def decrypt(d):
    out = bytearray(d); m = 0x655F; t = 0x4115
    for i in range(len(out)):
        out[i] ^= m & 0xFF; m = (m * t) & 0xFFFFFFFF
    return bytes(out)


def crilayla(src):
    assert src[:8] == b'CRILAYLA'
    usize, hsize = struct.unpack('<II', src[8:16])
    out = bytearray(usize + 0x100)
    out[:0x100] = src[16 + hsize:16 + hsize + 0x100]
    # bit reader reads the compressed stream backwards
    pos = 16 + hsize - 1
    bits = 0; nbits = 0

    def get(n):
        nonlocal pos, bits, nbits
        v = 0
        while n:
            if nbits == 0:
                bits = src[pos]; pos -= 1; nbits = 8
            take = min(n, nbits)
            v = (v << take) | ((bits >> (nbits - take)) & ((1 << take) - 1))
            nbits -= take; n -= take
        return v

    w = usize - 1  # write backwards into out[0x100:]
    base = 0x100
    while w >= 0:
        if get(1):
            off = base + w + 3 + get(13)
            ln = 3
            levels = (2, 3, 5, 8)
            done = False
            for lv in levels:
                v = get(lv); ln += v
                if v != (1 << lv) - 1:
                    done = True; break
            if not done:
                while True:
                    v = get(8); ln += v
                    if v != 255: break
            for _ in range(ln):
                if w < 0: break
                out[base + w] = out[off]
                off -= 1; w -= 1
        else:
            out[base + w] = get(8); w -= 1
    return bytes(out[:0x100 + usize])


def read_files(path):
    """Yields (name, data) for every file in the CPK, decompressed."""
    f = open(path, 'rb')
    hdr = utf_at(f, 0)[0]
    for r in utf_at(f, hdr['TocOffset']):
        name = ((r.get('DirName') or '') + '/' + r['FileName']).lstrip('/')
        f.seek(hdr['TocOffset'] + r['FileOffset'])
        data = f.read(r['FileSize'])
        if data[:8] == b'CRILAYLA':
            data = crilayla(data)
        yield name, data


def main():
    path = sys.argv[1]
    f = open(path, 'rb')
    hdr = utf_at(f, 0)[0]
    toc = utf_at(f, hdr['TocOffset'])
    if sys.argv[2] == 'list':
        for r in toc:
            print(r.get('DirName') or '', r['FileName'], r['FileSize'], r['ExtractSize'])
        return
    outdir = sys.argv[3]; filt = sys.argv[4] if len(sys.argv) > 4 else ''
    for r in toc:
        name = ((r.get('DirName') or '') + '/' + r['FileName']).lstrip('/')
        if filt and filt.lower() not in name.lower():
            continue
        f.seek(hdr['TocOffset'] + r['FileOffset'])
        data = f.read(r['FileSize'])
        if data[:8] == b'CRILAYLA':
            data = crilayla(data)
        dst = os.path.join(outdir, name)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, 'wb').write(data)
        print(name, len(data), r['ExtractSize'])


if __name__ == '__main__':
    main()
