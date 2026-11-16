#!/usr/bin/env python3
"""TFU-nx: read an XEX2 header (libraries, ids, compression) and, for basic compression, dump the decrypted image."""
import struct
import sys
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")

d = open(sys.argv[1], "rb").read()
assert d[:4] == b"XEX2"
flags, pe_off, _, sec_off, nopt = struct.unpack(">IIIII", d[4:24])
opts = {}
for i in range(nopt):
    k, v = struct.unpack(">II", d[24 + i * 8:32 + i * 8])
    opts[k] = v
print("module flags 0x%X  pe data at 0x%X  %d optional headers" % (flags, pe_off, nopt))
for k in sorted(opts):
    print("  key 0x%08X = 0x%08X" % (k, opts[k]))
if 0x00010201 in opts:
    print("image base 0x%08X" % opts[0x00010201])
if 0x00010100 in opts:
    print("entry 0x%08X" % opts[0x00010100])
if 0x00040006 in opts:
    o = opts[0x00040006]
    media, ver, basever, title = struct.unpack(">IIII", d[o:o + 16])
    print("title 0x%08X media 0x%08X version 0x%08X" % (title, media, ver))
if 0x000200FF in opts:
    o = opts[0x000200FF]
    size = struct.unpack(">I", d[o:o + 4])[0]
    n = (size - 4) // 16
    print("static libraries:")
    for i in range(n):
        p = o + 4 + i * 16
        name = d[p:p + 8].rstrip(b"\0").decode()
        a, b, c, qfe = struct.unpack(">HHHH", d[p + 8:p + 16])
        print("  %-10s %d.%d.%d.%d" % (name, a, b, c, qfe & 0x7FFF))
img_size = struct.unpack(">I", d[sec_off + 4:sec_off + 8])[0]
load = struct.unpack(">I", d[sec_off + 0x110:sec_off + 0x114])[0]
print("image size 0x%X" % img_size)
ff = opts[0x000003FF]
ffsize, enc, comp = struct.unpack(">IHH", d[ff:ff + 8])
print("encryption %d compression %d (1 basic, 2 normal/LZX)" % (enc, comp))
key_enc = d[sec_off + 0x150:sec_off + 0x160]
sess = Cipher(algorithms.AES(RETAIL_KEY), modes.ECB()).decryptor().update(key_enc)
data = d[pe_off:]
if enc:
    data = Cipher(algorithms.AES(sess), modes.CBC(b"\0" * 16)).decryptor().update(data[: len(data) // 16 * 16])
if comp == 1:
    nblk = (ffsize - 8) // 8
    out = bytearray()
    p = 0
    for i in range(nblk):
        ds, zs = struct.unpack(">II", d[ff + 8 + i * 8: ff + 16 + i * 8])
        out += data[p:p + ds] + b"\0" * zs
        p += ds
    open(sys.argv[2], "wb").write(out)
    print("wrote image", len(out))
elif comp == 2:
    open(sys.argv[2] + ".lzx", "wb").write(data)
    window = struct.unpack(">I", d[ff + 8:ff + 12])[0]
    print("LZX window 0x%X; wrote decrypted compressed stream" % window)
