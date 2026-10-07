# Native TSF adapter

`SampleIME/Server.cpp` creates `Service`, which loads `tiger-v2.tcd` beside the
DLL. The native engine, mapped indexes and user-word overlay run in the host
process. No resident Core or per-key IPC is involved. A context owns its engine
and TSF composition; immutable dictionary storage is reused within a process
and mapping-backed pages are shared across processes.

Windows file caches use absolute lexical paths, not `canonical()`: under
AppContainer, canonicalization can fail with access denied even when the same
Program Files dictionary can be opened and mapped. File opens still enforce
the original ACLs. `tests/AppContainerReadProbe.vcxproj` builds a read-only
regression probe accepting an existing AppContainer PID, a dictionary path,
and a sentence-model path. It impersonates that token to validate mapping,
cache reuse and missing-file rejection; it does not test foreground UWP input.
The service and manager resolve LocalAppData with
`KF_FLAG_NO_PACKAGE_REDIRECTION | KF_FLAG_DONT_VERIFY`, so packaged hosts use the same NativeTiger
root as desktop hosts. Installation grants AppContainer Modify access and a
low integrity label only to that data tree, including existing children.
External scheme sources and Program Files executables are not made writable.
New files inherit this policy. AppContainer configuration replacement retains
the inherited directory ACL when Windows cannot merge the old file's ACL;
desktop writers retain the original strict replacement behavior.

Theme menu actions write the shared configuration directly, without launching
a desktop manager from the restricted host. The existing data watcher applies
the change to other instances. The mode item declares both button and menu
capabilities and accepts right-click callbacks without a rectangle.
`AppContainerConfigProbe.vcxproj` tests shared known-folder resolution,
bidirectional config visibility, actual menu theme selection, repeated atomic
replacement and preservation of unrelated settings in a marked test directory.
The lookup test runs after impersonation with a null token argument, matching
the service call. Without DONT_VERIFY the shared child can be accessible while
the parent LocalAppData existence probe fails and returns an empty user root.
Taskbar popups use the current TSF context window as their owner. Foreground
behavior still requires a real UWP acceptance test. Creating
`menu-trace.enabled` in NativeTiger enables `menu-trace.log` with menu callback
stages and HRESULTs (no composed text); remove the marker to disable it.

2026-09-09 acceptance: the user confirmed the UWP/Start-menu fixes passed
foreground testing after installation, including the reported skin and menu
issues. ARM64, x64 and Win32 input regressions, restricted-token configuration
and theme-menu tests, and the desktop popup/focus fixture also passed.

Sentence continuation follows TigerClaw commit
`954c82d5ed3823a9007375568740a67d063dfebc`: empty-code automatic commit must retain
decoder-approved segmented duplicate-single paths, so `xrxbj`/`xryxbj` can keep
`反刍` as the first choice after `反` is committed. The decoder remains responsible
for the duplicate-single toggle, optimal-code eligibility and word-rank rules;
the session still rejects whole-input non-first edges on implicit continuation.
`tests/sentence_continuation_test.py` covers both prefix codes and both settings
on ARM64, x64 and Win32, using the real native decoder and Engine. This focused
regression tracks the upstream fix without replacing the older frozen oracle
with unrelated upstream changes.

Key preview copies the engine. Actual consumed keys request a synchronous TSF
write edit session, replace the composition or insert committed text, and publish
the new engine state. Unconsumed keys may still end an existing composition;
their observed event signature prevents duplicate dispatch. Enter's internal
history marker is suppressed when the physical Enter is passed to the application.
User operations are persisted only from actual dispatch, never from previews.

Composition termination clears engine state. Selection changes outside the
composition request an asynchronous edit session guarded by a context revision.
Deferred sessions retain the COM service. Candidate UI objects are detached from
the service before teardown so a host-retained UI element cannot call a dead
service. Focus changes refresh user data and reset transient modifier state.

The candidate object implements `ITfCandidateListUIElementBehavior` for hosts
that suppress the native window. Otherwise it renders a nonactivating popup and
commits mouse selections through a revision-checked edit session. Model refresh
does not depend on layout availability: missing geometry retains the last valid physical caret for the current
composition/owner, while continuing to render new candidates. There is no layout
timeout. If no valid caret has ever been obtained, the popup stays hidden; a
layout notification retries placement. Explicit host hiding, focus loss and
composition termination still hide/detach it. This lets wrapped Word preedits
remain usable even when both the insertion point and final-character queries fail.
Windows explicitly permits a missing-layout result from
[GetTextExt](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontextview-gettextext).
Read/write session constraints are documented under
[RequestEditSession](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontext-requesteditsession).

`tests/TsfHost.vcxproj` builds a native ARM64 application with real Windows TSF
documents and a small `ITextStoreACP`. It activates the registered profile only
for its process. The two documents have distinct HWND focus associations.
Tests exercise preview/dispatch, preedit replacement, commit/cancel, composition
termination across focus changes, UI-less selection and optional popup/mouse
selection. They require foreground focus and do not establish compatibility
with all applications. The optional second argument saves a native popup BMP.

Custom selection bindings now reload from the independent per-user file on
activation/focus and through the owning thread's directory-change scheduler;
see `../SELECTION.md`. Settings, user journals and schema generations also update
in the focused context without a focus transition. Notifications are polled at
250 ms with five-second reconciliation/reopen fallback. Actual delivery depends
on the host pumping messages and granting TSF edit sessions. Unchanged effective
data preserves paging and candidate UI. Installed validation remains pending.

Ordinary engine settings now load from the independent per-user config; see
`../SETTINGS.md` for the supported keys and unverified runtime coverage.

Remaining scope includes remaining configuration/schema UI, language-bar and input
mode compartment synchronization, configurable candidate layout/fonts, add-word
UI, persistence-error recovery, broader application/lifetime testing and the
remaining ordinary-engine features in `PLAN.md`.

## Private build validation

`tests/run_tsf_host.ps1 -PrivateBuild` uses a test-only activation manifest beside
`build/ARM64/Release/Tigirl.dll`. The activation context redirects COM only
inside the test process. It does not register or install that DLL, and the runner
compares the original registered path before and after. An existing enabled TSF
profile is still required for Windows profile metadata. The manifest is not part
of the installed package. See Microsoft's
[assembly manifest documentation](https://learn.microsoft.com/en-us/windows/win32/sbscs/assembly-manifests)
and [CreateActCtxW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createactctxw).

The fixture verifies that no other SampleIME generation loaded and that the real
service mapped `tiger-v2.tcd` from the tested DLL's directory. Merely preloading a
class factory is not accepted as service activation evidence. The full test still
requires OS foreground focus and does not skip that requirement.

Add `-ActivationOnly` when checking startup without an interactive desktop. This
mode verifies profile/service activation, the module and dictionary, then
teardown; it explicitly reports zero input events and never claims input/window
coverage. It currently passes on ARM64 with registration unchanged. The attempted
full private run stopped before event zero because GetForegroundWindow returned
null. Full composition, layout-recovery, font and layered-window behavior remain
unverified for the pending build until an interactive desktop is available.

## Tab confirmation in sentence input

Ported from TigerClaw `14b611f48555bb0f02b1fdf1e7b02671096bebbc`.
Tab/Shift+Tab highlights without committing. The next code letter fixes the
chosen text and raw-code boundary; the decoder scores only the remaining tail,
with language-model and supplement context reconstructed from the locked text.
With automatic commit enabled, that letter also submits only the uncommitted
selected text, independently of confidence and retained-code floors. Otherwise
Backspace reaching a lock boundary releases it; nested locks unwind one at a
time. Numeric/punctuation rank selectors edit the current segment, and arrow
navigation alone does not arm confirmation. Literal exits retain live raw-code
semantics. Immutable lock snapshots travel with synchronous/asynchronous decode
tickets and exact-path queries; old completions cannot replace the new state.

Run `tests/sentence_tab_lock_test.ps1` in Windows PowerShell to build and run
ARM64/x64/Win32 engine+decoder fixtures, the preceding rumination regression,
and the session-state suite. These tests replay pending decode requests and
inject stale results; they do not constitute physical foreground input testing.

## 用户皮肤目录

候选皮肤统一读取 `%LOCALAPPDATA%\Tigirl\皮肤`，与 `码表` 并列。
设置和菜单的“打开皮肤目录”会创建该目录；放入 `.ssf` 后点击刷新即可。
设置了绝对路径 `NATIVE_TIGER_USER_ROOT` 时，使用其下的 `皮肤` 子目录。
目录解析不创建文件；缺少默认皮肤文件时保留内置默认外观。
x64 安装包通过现有码表用户数据合并流程分发默认皮肤，保留用户修改。

SSF 背景分块边界按当前 DPI 对齐到物理像素，避免缩放后出现透明接缝。
Windows 编译 `tests/SsfRenderProbe.vcxproj` 后，可运行
`python tests/ssf_seam_test.py --probe build/tests/ARM64/ssf_render_probe.exe`
检查不透明/半透明、拉伸/平铺、多种 DPI 及小尺寸背景的像素连续性。

SSF 解析只校验候选窗使用的 General、Display 和 Scheme 配置字段，
跳过未使用的 StatusBar 等节内字段及遗留尾部文本；候选窗配置格式错误仍会拒绝加载。
独立 custom 装饰层目前尚未绘制，因此部分皮肤只能显示背景和文字。

设置中的字体决定中文和英文的字体，SSF 的 `font_size` 决定原始排版基准，皮肤原图画布、边距和固定切片边缘共同决定内容尺寸。背景不缩到原图以下，避免原始文字边距落出底框；内容超出时按皮肤规则扩展。
设置字号只控制整体比例：`设置字号 / 皮肤 font_size`。文字、背景、留白、边距、间隔及鼠标命中区域一起缩放，再应用屏幕 DPI。
字体通过设置选择，缺少皮肤字号时采用 17。皮肤外观和皮肤动画默认开启，设置中不再提供开关，也不读取旧开关。
可在 `[Display]` 中使用虎娘扩展 `candidate_spacing`（横排候选间距）、`line_spacing`（竖排行间距）、`character_spacing`（额外字距），单位为皮肤原始 DIP，范围 0–200，省略为 0。这些是虎娘扩展，不是所有 SSF 都具备的标准字段。

候选布局下拉框只有五项：横排带编码（H1）、竖排带编码（V1）、横排无编码（H2 候选部分）、竖排无编码（V2 候选部分）、仅编码（H2 候选部分）。
配置值依次为 2、4、5、6、7，默认 6；已删除的 1、3 不再生效。布局快捷操作按这五项循环。
两种无编码模式空码时，在候选位置绘制无序号、不可点击的编码占位文本。“仅编码”始终在横排候选面板的候选位置显示编码，不显示候选字。
候选窗动效固定开启，使用默认时长 100 毫秒，不再读取旧动效开关和时长配置。候选外观页不显示皮肤诊断说明。
