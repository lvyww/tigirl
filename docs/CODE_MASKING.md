# 编码伪装

在“输入设置 → 候选外观”填写“编码伪装”，留空关闭。缺省为空，安装不替
用户启用。设置使用原版键名 `编码伪装`，同时作用于候选窗编码和应用中的
TSF 预编辑文字。字典查询、候选正文/注解、快捷键、实际提交文字仍使用
未经替换的原始数据。

| 替换串 | 活动编码 | 显示 |
| --- | --- | --- |
| ● | ab | ●● |
| 甲乙丙 | abcd | 甲乙丙甲 |
| 甲😀 | ab | 甲😀 |

规则来自原版 Core/ProtocolHandler.cs 的 MaskInputBufferForDisplay 与
BuildDisplayComposition：替换串按 .NET StringInfo 对应的 Unicode 字素切分；
活动编码按 UTF-16 码元处理，小写化后以 `abcdefghijklmnopqrstuvwxyz;` 查位置，
按替换串长度取模；不在表中的字符使用首项，空白原样保留。emoji ZWJ 序列、
组合附加符、旗帜属于完整替换单元。此为显示替换，不是加密。

Engine::snapshot 新增已解析前缀长度，普通混合输入保留该前缀；整句模式使用
已有 displayCode 作为活动编码，与原版对应。公共 displayComposition 同时供
CandidatePresentation 和 TSF Service::apply 使用，不把显示结果写回引擎 raw。
TSF 范围长度按替换后的 UTF-16 长度设置，因此多码元替换的退格仍由原始编码
驱动。整句解码、候选选择及提交路径不做字符替换。

配置读取修正了行尾空白被过早剥除的问题：保留分隔符再解析，后置空值可以
覆盖之前的同名设置，与 CoreRuntimeState 的读取方式一致。

验证：tests/code_mask_test.py 覆盖 Unicode、未知字符、空白、混合前缀、整句
分隔及候选正文不变；tests/input_settings_test.py 覆盖保存、重开和取消。
带 .tsf-candidate-mask-test 的临时 TSF 宿主验证甲😀预编辑、退格、恢复、
UI-less 原始候选、鼠标操作后提交交。测试报告明确区分实际完成与焦点前置
检查失败，不以旧报告替代本轮结果。

本轮 `tests/code_mask_tsf_test.py` 在三架构真实 TSF 编辑会话中通过：伪装预编辑、
多码元退格/恢复、UI-less 候选不变、空格提交、混合输入前缀与提交、取消。
它通过显式服务激活及定向回调测试，不宣称实际键盘前台路由通过。
`candidate_mouse_test.py` 本轮尝试停在 OS 焦点前置检查；普通应用的物理键鼠
体验仍待用户实测。设置的 144 DPI 截图已检查，无控件重叠。
