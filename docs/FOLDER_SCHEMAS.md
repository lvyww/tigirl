# Folder-based schema management

The normal manager now follows TigerClaw's directory selection workflow. Set
“码表目录” to the root containing schema subdirectories, select a folder name,
and click “使用方案”. “刷新列表” discovers added folders; “打开” opens the root.
After editing text tables, “重新加载” prepares and selects the displayed schema.
The first load builds a native binary automatically. Subsequent loads compare
SHA-256 content fingerprints and reuse unchanged validated caches. Ordinary TSF
input continues to map the same read-only binary; no per-key helper or IPC was added.

The default root is `%LOCALAPPDATA%\Tigirl\码表`. The selected root is saved
in `config.txt` as `码表存储位置`. To use the existing TigerClaw tables, browse to
`C:\Users\yc\Desktop\bime_codex_src_20260513\release_arm64\码表` (the root, not
one individual schema). Source directories are only read, never rewritten.
Pinyin defaults to `拼音反查码表` next to that root. An optional absolute
`拼音反查目录` config value overrides it. A missing pinyin directory is supported.

The source folder name remains the scheme name, including `虎码字词`. A prepared
user generation with that name overrides the bundled binary while retaining
`user/tiger-words.tcu`; other schemes retain their independent `user.tcu` journals.
Without a prepared override, `虎码字词` continues using the bundled dictionary.
The existing compiled-schema list remains selectable for compatibility.

Cache preparation uses the short-lived importer `--ensure`, a serialized import
lock and immutable generation files. Source content (including pinyin), names,
enumeration order, locale and importer format version determine cache identity.
Timestamp-preserving edits invalidate the cache. A second source fingerprint is
checked after compilation. Binary and journal validation precede generation
publication. Import failures preserve the previous descriptor and selection.
Unchanged cache reuse still validates the binary and replays the journal.

“更多维护” opens the previous import/version/user-journal tools in a separate
window. These operations are no longer required for ordinary source-folder use.
This change does not introduce automatic filesystem watching of source text while
typing: after editing source files, use “重新加载”. It does not implement TigerClaw's
sentence engine or make arbitrary folder-specific engine settings compatible.

Validation:

- `tests/schema_folder_test.py`: actual hidden Win32 manager and helper processes;
  new-folder selection, built-in name override, journal preservation, cache reuse,
  same-timestamp content change, pinyin change, missing-source failure, corrupt
  cache rejection/rebuild, and layout at 96/144/192 DPI.
- `tests/schema_manager_test.py`: existing advanced import/update/restore,
  missing companion, corrupt schema and close-during-worker behavior.
- `tests/schema_select_test.py`: Unicode/case canonical selection and failure preservation.
- `tests/staged_tsf_activation.py --builtin-override`: native ARM64 and ARM64X
  ARM64/x64 TSF instances must map the override's actual Windows backing path;
  also exercises mode, focus and edit-lock regression checks.

Deployment: ARM64X generation `5aa4bd69945cfb9c` and matching x86 companion
installed successfully. Primary artifact hashes and the x86 DLL match their
installation records; ARM64/x64 registered TSF checks pass. Evidence:
`build/folder-installed-validation.json`. The installed normal manager was opened
and its fully initialized 150% DPI window captured and visually inspected
(`build/folder-manager-installed.png`). No keyboard or mouse input was injected.
