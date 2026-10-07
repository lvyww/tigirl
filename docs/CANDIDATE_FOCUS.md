# 候选窗焦点与系统快捷键（2026-10-03）

## 运行约定

未绑定输入法功能的 Win 组合键直接透传，不主动取消 composition；Ctrl/Alt 原有编辑行为保持不变。
系统截走组合键中的普通按键时，松开剩余 Shift 不应误触发中英文切换。

临时失焦只隐藏窗口、暂停动画和点击交互，不清编码、选择、分页、锁定和解码版本。
`Service::hideUI()` 暂停当前 CandidateUI；`closeUI()` 才会 detach、销毁 HWND 和结束 UIElement。
只有真正的结束事件执行 close。EndUIElement 重入不能清掉较新的 UI。

CandidateUI 分别保存宿主的 Show 请求和当前焦点状态，二者都允许时才显示。
重复焦点通知只更新必要的位置，不重建未变化的候选模型；焦点恢复不覆盖宿主 Show(FALSE)。
已失效的上下文／composition 回调直接返回，不隐藏新上下文候选。延迟点选在失焦后失效。

失焦期间完成的有效整句解码只保存在内存，不改文档或发布候选；恢复时同步 preedit 和候选。
输入编辑、资源变化、取消与上下文移除仍使旧结果失效。候选排序、学习日志、模型格式未修改。

末端 caret、末字符回退、最后有效坐标、DPI 和工作区限制仍保留。
owner/root 矩形只是上下方向记忆的辅助信息：查询失败时放弃方向记忆，不隐藏有有效 caret 的候选。
删除 CandidateUI::update 的未使用 layoutPending 参数及 Service 中后续无用途的标记赋值。

本次没有截图进程白名单、隐藏延时补丁、强制失焦显示或额外置顶措施。

## 回归入口

Linux/WSL：

```sh
python3 tests/candidate_focus_test.py --negative-control
python3 tests/candidate_selection_test.py
python3 tests/sentence_learning_test.py
python3 tests/candidate_orientation_test.py --cxx clang++ --sanitize
```

Windows MSVC developer shell（先编译对应 DLL、LexiconImport 和两个 Host 工程）：

```powershell
python tests/candidate_focus_test.py --cxx cl --negative-control
python tests/candidate_ui_layout_test.py --negative-control
python tests/candidate_ui_presentation_test.py --negative-control
python tests/candidate_orientation_test.py --cxx cl --ui
python tests/candidate_host_dpi_test.py
python tests/candidate_layout_test.py --platform x64
python tests/candidate_selection_test.py --cxx cl
python tests/candidate_sentence_focus_test.py --platform x64
```

2026-10-03：上述 x64 八组测试退出码均为 0。焦点引擎 23 项并通过三个负向对照；
UI/布局 412 项；动效 38 个用例、1123 项；方向 79369 项及 11 个窗口用例、253 项；
候选选择 3421 项；Linux 学习/性能 25996 项及 16 进程并发检查通过。
真实 TSF 普通候选覆盖两个内置方案，整句测试覆盖后台完成、延迟回调及恢复后手动选择。
整句集成 fixture 关闭学习以固定排序；学习证据保留由生产 Engine/SentenceSession 测试验证。
x64、ARM64、Win32 DLL 编译通过。DPI 实测只连接一个显示器。

原有资源清单漏记 5f39d84 新增的三个注释/拆分文件，已补入哈希；其余文件哈希不变，未改码表内容。
候选选择测试改用现有有限重试清理工具，避免 Windows 临时 EXE 占用掩盖实际结果。

本机完整日志和哈希：`build/candidate-focus-validation-v5ehf_6a/VALIDATION.json`。
测试使用隔离数据及显式 TSF 按键回调；不注册输入法，不操作用户应用。
未替换已安装程序，未进行真实 Win+Shift+S 截图像素验收。
普通失焦仍隐藏窗口，截图是否包含候选取决于取图与焦点通知的时序，不能仅据此宣称截图问题完全解决。
