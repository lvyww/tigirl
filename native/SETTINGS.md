# Ordinary engine settings

The pending native build reads `%LOCALAPPDATA%\Tigirl\config.txt` on TSF
activation and document focus. It keeps the daily TigerClaw configuration separate.
A missing file uses defaults. New contexts use 默认中文; reloading an existing
context preserves the user's current Chinese/English mode and resets its page.
Normal key preview/dispatch does not read the settings file.

A host may set `NATIVE_TIGER_USER_ROOT` to an absolute Windows directory before
activating the TIP. This redirects its configuration, selection bindings and
user-word journal together; dictionaries and fonts still come from the DLL's
directory. The default remains LocalAppData/Tigirl. Secure activation does
not read user data or this override. A relative/oversized override fails
activation rather than falling back to a different user's data location.
The mixed TSF fixture uses this process-local option with a disposable directory.

The following original Chinese keys are connected to the engine:

| Key | Default |
| --- | --- |
| 默认中文 | 是 |
| shift切换中英文 | 是 |
| Ctrl+空格切换中英文 | 是 |
| Ctrl+等号手动加词 | 是 |
| 手动加词快捷键 | Ctrl+VK_OEM_PLUS |
| 中文状态下使用英文标点 | 否 |
| /输出顿号 | 是 |
| 回车清屏 | 否 |
| TAB清屏 | 是 |
| 空码自动清屏 | 是 |
| 最大码长无重自动上屏 | 是 |
| 中英文不限长混合输入 | 否 |
| `键拼音反查 | 是 |
| 分号次选 | 是 |
| 引号三选 | 是 |
| 显示注释 | 是 |
| 显示拆分 | 否 |
| 最大码长 | 4, clamped to 1–16 |
| 每页候选个数 | 5, clamped to 1–10 |
| 翻页键 | - = |

Separate the key and value with a tab, space or comma. The first separator wins;
leading/trailing whitespace is ignored, whole-line `#` comments are supported,
and the last occurrence of a key wins. `=` is not a key/value separator.
Booleans accept 是/否, true/false, on/off, 1/0; Latin spellings are case-insensitive.
Empty or invalid values use that setting's default. Integer overflow also uses
the default; representable integers are clamped. Page-key strings are exact:
`[ ]`, `Shift Tab/Tab`, `PageUp/PageDown`; other values use `- =` behavior.

The common bounded Unicode file reader supports UTF-8 and BOM-marked UTF-16/32.
Malformed Unicode/read failures currently produce debug output and use defaults.
Reading never rewrites the user's file. The 隐藏候选 action now updates its
setting through `ConfigStore` (see `CONFIG_STORE.md`). General settings UI/error
recovery remain pending.
The keys above and the candidate options in `CANDIDATES.md` are applied in this
build. Manual add-word shortcuts and their conflict rules are described in
`ADDWORD.md`. Ordinary mixed input is described in `MIXED_INPUT.md`.
Schema switching, remaining action shortcuts and other
original options still need integration; copying an entire original config does not enable them.

`tests/settings_parity.py` compares actual original configuration reload/getter
results against native parsing for 224 cases on Linux and Windows ARM64. This
verifies parsing/defaults, not every resulting input operation or installed TSF
reload. Original source is unchanged and runs only in an isolated filesystem and
HKCU registry sandbox. See `SELECTION.md` for the separate selection-key file.

`tests/settings_key_parity.py` additionally exercises six non-default profiles
(default English with toggles disabled, English punctuation, clear/auto-commit
options, compact and wide pages, and all supported paging choices). Each has
6,850 native key events compared with original Core; the oracle now applies
`GetDefaultChinese()` through `SetChinese()` at initialization, matching original
`ProtocolHandler`. Reference source is unchanged. Replays verify config, selection
file and trace fingerprints before using cached oracle results.

An editable example containing only the connected settings is provided at
`data/native-config.example.txt`. Settings UI, remaining candidate options
and installed application checks remain outstanding.
