# Custom selection keys

The native TSF source reads UTF-8 (optional BOM), or UTF-16/UTF-32 with a BOM, from
`%LOCALAPPDATA%\Tigirl\自定义选重键.txt` on activation and document focus.
It does not read or rewrite the daily TigerClaw installation. Create the file
with, for example:

```
# Replace candidate 2's key list with right Shift and F2
2选 VK_RSHIFT VK_F2
# An empty list disables the default 3 key
3选
```

Supported syntax follows the original Core parser: `1选` through `10选`, followed
by space/tab-separated decimal integers, `0x` hexadecimal integers, or the
original `VK_*` names (case-insensitive). Only whole-line `#` comments are allowed.
Unspecified candidate numbers keep their defaults: 1..9 and 0 for candidate 10.
The last row for a candidate replaces its entire list. Repeated keys in a row
are deduplicated; a key assigned to several candidates selects the lowest number.
Invalid syntax anywhere rejects the whole file and the service uses defaults.
A missing file also uses defaults. The native settings UI edits individual
bindings and preserves other rows during concurrent updates. The parser preserves signed 32-bit numeric values
for round trips, but values outside physical VK range 0..255 cannot dispatch.

The file is bounded to 4 MiB. Malformed Unicode is rejected; the original .NET
reader can replace malformed sequences, so malformed-file behavior may differ. The pure parser has 208 oracle cases, including all original
named keys, overwrites, empty rows, shared bindings, comments, CR/LF, Unicode
edge whitespace, signed/hexadecimal bounds and invalid files. Tests run on Linux
and Windows ARM64. This parser evidence does not yet prove custom physical-key
behavior in every application. Reserved-key priority remains governed by the
engine, just as the original input dispatcher can preempt a configured binding.

Build `tests/SelectionProbe.vcxproj` for Release/ARM64 and run
`python3 tests/selection_parity.py` for differential evidence. The oracle invokes
private static methods from the unchanged source snapshot under the existing
filesystem/registry isolation. The shipped native DLL never loads that oracle.

`python3 tests/selection_key_parity.py` exercises four binding profiles against
original-Core key dispatch (17,696 events); add `--windows --replay` to replay
against the native ARM64 engine after producing the oracle traces. CapsLock is
eligible for modifier selection. In pinyin mode, Space, paging and built-in
semicolon/quote selection take priority over custom bindings; custom bindings
precede Backspace, Enter, Tab and Escape. Both platform traces pass. This does
not replace real application/TSF validation of the pending build.

The selection editor opens malformed syntax or Unicode with default bindings
and an explicit recovery notice. Cancel leaves the original bytes unchanged.
Saving a repair takes the stable configuration lock, compares the exact bytes
against the editor's opening snapshot, preserves a uniquely named adjacent
`.invalid.{GUID}` backup, and atomically publishes valid UTF-8 bindings. A
concurrent byte change requires reopening the editor. Read/access failures or
files over the size limit do not enter this repair flow. This is deliberate
user-initiated recovery; ordinary TSF loading still rejects malformed Unicode.
`tests/input_settings_test.py` covers native controls, exact backups, cancel,
concurrent byte changes and denied reads for four broken Unicode fixtures.
Physical interaction and rendered acceptance remain separate pending checks.
