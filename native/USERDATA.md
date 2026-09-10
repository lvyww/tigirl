# User words and immutable snapshots

`Lexicon` overlays only edited main-table codes on the shared `Dictionary`.
Unmodified candidates and all auxiliary sections remain mapping-backed views.
Each changed code owns its candidate vector; older snapshots retain their previous
order. Candidate identity is the commit text, preserving stored display aliases
for top/advance operations. Main-table exact keys remain present after deletion
of their last word, matching the reference's short-symbol metadata.

`Engine` uses a `Lexicon` snapshot. Ctrl+number tops a word,
Ctrl+Shift+number deletes it, and Alt+number moves it one position forward.
Repeated key-downs perform an operation once until key-up. The engine changes
only its own snapshot and queues `UserChange` values. A copied engine can preview
TSF key handling without writing user data. The actual adapter must drain
`takeUserChanges()`, call `UserStore::commit()`, then install its returned snapshot
to incorporate concurrent writers. A persistence error must be surfaced by the
adapter; engine previews must never call the store.

`UserStore` uses a separate per-user, per-schema journal. It takes an exclusive OS
file lock (`LockFileEx` on Windows, `flock` on Linux), reads the latest committed
operations, rebases new edits, appends checksummed records, and flushes the file.
Independent hosts therefore merge updates instead of replacing each other's
files. No broker or resident Core is needed. The adapter should refresh on focus
or a file-change notification, not on every keystroke. Locks are released by the
OS if a process exits. The journal itself is never renamed while locked.

Journal format: 8-byte `TIGERU01` magic; then records consisting of a little-endian
u32 payload length, u32 CRC-32/ISO-HDLC, and payload. Payload is u32 operation
(add=0, delete=1, top=2, advance=3), u32 code length, u32 packed-text length, then
the two raw UTF-16LE strings. Lengths count UTF-16 code units. Packed text is not
backslash escaped, so aliases, line breaks and supplementary characters survive
replay. Code normalization occurs through the same `Lexicon::changed` path.

A final incomplete record is ignored on read and removed before the next append.
A complete record with an invalid checksum or structure causes an error and is
never silently discarded. An interrupted first header is accepted only if its
bytes are a prefix of the exact magic. Records are bounded to 16 MiB and journals
to 128 MiB. Refresh currently replays the journal; compaction and notifications
remain future lifecycle work. The TSF DLL now drains actual dispatch
changes into this store and refreshes on focus. The current journal is
`%LOCALAPPDATA%/Tigirl/user/tiger-words.tcu`. Adjustment failures produce
debug output/a beep and a language-bar warning icon/text/tooltip. The tooltip
asks the user to check storage and explicitly repeat the adjustment. The warning
persists through ordinary typing and reads, clearing after a successful user-word
write or schema switch; it ends when the service deactivates. It does not queue or
automatically retry the failed operation. Durable retry and general recovery UI
still need work.
If append or flush fails, the store now attempts to truncate the entire attempted
batch back to the previous valid length and flush that rollback while retaining
the exclusive lock. A rollback failure is reported explicitly; no automatic
replay is attempted in that ambiguous state. This does not make process crashes
or power loss atomic across a multi-record batch.

After a failed TSF adjustment, the adapter reloads the journal if possible and
restores persisted candidate ordering. If reload is also unavailable, it uses
the last known persisted snapshot. It no longer leaves an unpersisted local
reorder visible. Unavailable storage follows the same error path. Failed
requests are not durably queued; retry/recovery UI remains unfinished.
The native add-word dialog now commits through this same store, reports save
errors without closing, and supports retry. See `ADDWORD.md` for validation and
remaining real-application checks.

Validation: `tests/lexicon_parity.py` compares 27 actual original-Core mutations,
including aliases and empty-key prefix behavior. `tests/key_parity.py` includes
physical adjustment shortcuts and repeated events. `tests/user_store_concurrency.py`
launches four independent writers, verifies all 48 entries after restarting,
tries all 71 interrupted-tail byte positions for its final record, verifies new
appends recover each tail, and rejects complete-record corruption without writes.
Use `--windows` for the native ARM64 executable.

`tests/user_store_short_write.cpp` additionally uses Linux `RLIMIT_FSIZE` to
force actual short writes at all 88 byte positions of a two-Advance batch.
Every failed append restores the exact previous journal bytes; retry produces
the same journal as one uninterrupted batch. Report:
`build/user-store-short-write-linux.json`. This exercises write failures and
successful rollback, not failing fsync/rollback or Windows short-write faults.
The private ARM64 TSF add-word fixture denies journal writes with an actual
Windows file-sharing lock and verifies candidate order and subsequent commit
after Ctrl+2 fails in both UI modes.
