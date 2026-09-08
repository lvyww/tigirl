# TigerClaw immutable dictionary v2

All integers and UTF-16 code units are little endian. Strings have explicit
lengths, no terminators, and may be interned. Offsets are absolute byte offsets;
there are no process pointers or architecture-dependent structs.

Header (32 bytes): magic `TIGERD02` (8), version u32 (2), section count u32 (6),
file length u64, quick-symbol flags u32, reserved u32 (0). Quick-symbol bits are
semicolon/slash/left bracket/z (1/2/4/8).

Six section descriptors (16 bytes each): ID u32, record count u32, record offset
u64. IDs: main=1, pinyin=2, comment=3, split=4, full code=5, construct code=6.

All section records follow contiguously in section order. A record is 32 bytes:
key offset u64, key UTF-16 length u32, flags u32, values offset u64, value count
u32, reserved u32 (0). Main keys include nonterminal prefixes without candidates.
Main flags: unique terminal=1, nonterminal prefix=2, automatic short symbol=4, exact source key=8.
The exact-key bit is retained for empty source entries: these affect short-symbol
metadata after user deletions, unlike synthetic prefixes. v1 lacks this information
and must be re-exported; the v2 reader deliberately rejects it.
Records are strictly sorted by UTF-16 ordinal key, matching .NET Ordinal.

All values follow contiguously in record order. Each is 16 bytes: string offset
u64, UTF-16 length u32, reserved u32 (0). The remaining bytes form the string pool.
Main/pinyin values retain upstream packed display/commit separators and order.
Other sections have one value per key.

Publish new files for new generations; never modify a mapped generation in place.
The reader validates all directories, string ranges, key ordering and flags before
exposing a mapping. Full indexes and string data remain in the read-only mapping.
