#!/usr/bin/env python3
# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Wrap a flat binary into a UF2 file at a given load address and family ID.
# Drop-in replacement for arduino's bin2uf2 utility:
#   bin2uf2 <address> <familyid> <input.bin> <output.uf2>

import struct
import sys

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END    = 0x0AB16F30
UF2_FLAG_FAMILY  = 0x00002000
BLOCK_PAYLOAD    = 256


def parse_int(s):
    return int(s, 0)


def main(argv):
    if len(argv) != 5:
        sys.stderr.write("usage: bin2uf2 <address> <familyid> <input.bin> <output.uf2>\n")
        return 2

    addr = parse_int(argv[1])
    family = parse_int(argv[2])
    with open(argv[3], "rb") as f:
        data = f.read()

    total = (len(data) + BLOCK_PAYLOAD - 1) // BLOCK_PAYLOAD
    with open(argv[4], "wb") as out:
        for i in range(total):
            chunk = data[i * BLOCK_PAYLOAD:(i + 1) * BLOCK_PAYLOAD]
            payload = chunk + b"\x00" * (BLOCK_PAYLOAD - len(chunk))
            header = struct.pack(
                "<IIIIIIII",
                UF2_MAGIC_START0,
                UF2_MAGIC_START1,
                UF2_FLAG_FAMILY,
                addr + i * BLOCK_PAYLOAD,
                BLOCK_PAYLOAD,
                i,
                total,
                family,
            )
            out.write(header)
            out.write(payload)
            out.write(b"\x00" * (476 - BLOCK_PAYLOAD))
            out.write(struct.pack("<I", UF2_MAGIC_END))

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
