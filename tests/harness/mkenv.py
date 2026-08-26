#!/usr/bin/env python3
"""Build and read a file-backed U-Boot environment.

The harness needs an environment the client can open like the device's: the
same on-disk shape, so the binary under test does the same parsing it does in
the field. Non-redundant layout -- a little-endian CRC32 over the data area,
then NUL-separated key=value pairs, NUL-NUL terminated, zero padded.
"""
import binascii
import struct
import sys

ENV_SIZE = 0x2000


def build(path, pairs, size=ENV_SIZE):
    body = b"".join(("%s=%s" % (k, v)).encode() + b"\0" for k, v in pairs.items())
    body += b"\0"
    if len(body) + 4 > size:
        raise SystemExit("environment does not fit in %#x" % size)
    body = body.ljust(size - 4, b"\0")
    crc = binascii.crc32(body) & 0xFFFFFFFF
    with open(path, "wb") as fh:
        fh.write(struct.pack("<I", crc) + body)


def read(path):
    with open(path, "rb") as fh:
        blob = fh.read()
    stored = struct.unpack("<I", blob[:4])[0]
    body = blob[4:]
    if (binascii.crc32(body) & 0xFFFFFFFF) != stored:
        raise SystemExit("checksum mismatch: the client left the environment unreadable")
    end = body.find(b"\0\0")
    out = {}
    for entry in body[: end if end >= 0 else len(body)].split(b"\0"):
        if b"=" in entry:
            k, v = entry.split(b"=", 1)
            out[k.decode()] = v.decode()
    return out


if __name__ == "__main__":
    if sys.argv[1] == "build":
        pairs = dict(a.split("=", 1) for a in sys.argv[3:])
        build(sys.argv[2], pairs)
    elif sys.argv[1] == "size":
        # The environment's size belongs to the format, so the driver asks for
        # it rather than repeating the number in its own configuration line.
        print("%#x" % ENV_SIZE)
    elif sys.argv[1] == "read":
        for k, v in sorted(read(sys.argv[2]).items()):
            print("%s=%s" % (k, v))
    else:
        raise SystemExit("usage: mkenv.py build <file> k=v...  |  read <file>  |  size")
