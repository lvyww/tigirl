# Reference oracle

`upstream/` is a byte-for-byte snapshot of TigerClaw Core and Shared C# source
from the reference checkout on 2026-09-08. SHA-256 values are in
`upstream-sha256.json`. Do not edit these files to make parity checks pass.

`Oracle.cs` supplies a separate entry point. It runs the original table loader
and key engine in a staging directory, without starting the pipe server, Overlay,
autorun, or the installed TigerClaw Core. The staging marker is mandatory because
the original loader may persist configuration and recent-table metadata. Before
initialization, RegistrySandbox redirects this process's HKCU to a temporary
oracle-only subtree using RegOverridePredefKey: the original loader's autorun
synchronization must never reach the real user Run key.

The export operation produces immutable `.tcd` data and an independent JSONL
enumeration for exhaustive native-reader checks. The trace operation runs physical
key events through the original engine and captures results and UI state.
The changes operation compares live user-word mutations. Both trace and changes
may append adjustment files; run them against disposable filesystem copies of
the staging baseline. `tests/key_parity.py` and `tests/lexicon_parity.py` create
and clean those copies automatically. Export now writes dictionary v2, including
the original exact-key inventory even for zero-candidate entries.

This utility is build/import/test infrastructure. The TSF DLL must never load
the managed engine or communicate with it during typing.
