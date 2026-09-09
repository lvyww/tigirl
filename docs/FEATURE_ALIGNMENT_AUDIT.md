# TigerClaw detail-feature audit — 2026-09-09

Scope: current working source, compared with readonly sibling
`bime_codex_src_20260513/next`. This is a source audit, not a claim that every
feature has passed physical application tests or every built change is installed.

| Feature | Native status | Reference/evidence |
| --- | --- | --- |
| 编码伪装 | Now implemented in source; see CODE_MASKING.md for validation | Core/ProtocolHandler.cs: MaskInputBufferForDisplay and BuildDisplayComposition; CoreRuntimeState.GetCodeMasking |
| 开启打字音效(娱乐), 按键音量0~100 | Missing | Core publishes SoundSeq/SoundVk/volume; Overlay/TypingSoundPlayer.cs plays KeyNormal/KeySpace/KeyFunc.wav |
| 使用剪贴板上屏, 白名单 | Missing compatibility path; evaluate only for demonstrated TSF-host problems | CoreRuntimeState and BuildHookNativeConfigExtraJson; native Service uses TSF edit sessions |
| 候选窗动效 | Reference UI retains label/order, but inspected current Overlay candidate path cancels animations and sets opacity/translation directly | ConfigSettingOrder.cs, MainWindow.Candidate.cs; do not classify a working animation as missing solely from its label |
| 开机自动启动, Alt+反斜杠外挂开关, 自动切换系统语言 | Architecture-specific; no direct requirement for pure TSF | Original controls Core/Hook lifetime and underlying keyboard layout |
| 隐藏状态栏 | Original floating status window has no exact native counterpart | Native uses standard TSF language-bar/taskbar item |
| 整句神经重排 | Not ported, outside the agreed non-Qwen sentence implementation | Original Core option; not a small display feature |
| 中键模式切换、候选延迟、注解延迟、右键、滚轮字号 | Implemented in current source; latest middle-click frontend test pending focus availability | CandidateUI, CandidateReveal, ConfigStore, renderer and tests |
| Ctrl+数字置顶、Ctrl+Shift+数字删除、Alt+数字前移 | Implemented for ordinary composing mode | native/Engine.cpp user-change dispatch; UserStore persistence |
| 日期/时间/星期、随机项、金额大写、历史辅助加词 | Implemented | native/DynamicText.cpp, UppercaseText.cpp, History.h and accompanying tests/docs |

Masking semantics: empty value disables; split the replacement string into Unicode
text elements, map lowercase `abcdefghijklmnopqrstuvwxyz;` by position modulo
replacement count, preserve whitespace, and map other characters to replacement
index zero. This is a string option, not a boolean. For mask `●`, `ab` displays
`●●`; for mask `甲乙丙`, `abcd` displays `甲乙丙甲`. The display prefix is preserved
while only active code is masked. The original applies this display string to
both frontend preedit responses and candidate-window state; changing only the
candidate label would leave incomplete parity. Actual lookup and committed
candidate text must continue using original code.

Recommendation: implement masking first (grapheme handling, prefix separation,
preedit and candidate consistency, unmasked commit). Typing sound is optional
next. Clipboard compatibility should follow an actual affected-application case.
