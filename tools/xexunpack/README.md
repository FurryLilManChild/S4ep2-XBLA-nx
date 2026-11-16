# lzxd_mem

Decompresses a dechunked XEX LZX stream with the SDK's libmspack. Build (Linux):

    M=sdk/thirdparty/libmspack/libmspack/mspack
    gcc -O2 -I$M -o tools/xexunpack/lzxd_mem tools/xexunpack/lzxd_mem.c $M/lzxd.c

Used by `tools/xex_unpack.py`.
