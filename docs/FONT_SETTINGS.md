# 字体选择与预览

字体选择改为下拉列表，参照虎爪 `TigerClaw.Dialog/ConfigWindow.xaml.cs` 的
`SetupFontEditor`、`AddFontItem`、`SelectFontComboByValue`、
`GetPreferredFontDisplayName` 和 `SyncFontComboPreview`。

- 优先列出管理程序旁“字体”目录中的 TTF 字体，名称带 `#` 前缀。
- 随后列出 DirectWrite 系统字体集合，按字体家族名称忽略大小写排序。
- 优先显示 zh-CN 名称，其次 en-US 名称；系统字体仍有其他名称时使用该名称。
- 显示名称忽略大小写去重，并接受原有英文家族名作为配置别名。
- 配置字体不存在时选择第一项；保留原生候选窗已有的默认字体和字号。
- 下拉列表的每一项使用自身字体绘制；选中框也使用选中字体，而非统一 UI 字体。
  使用 DirectWrite / Direct2D 绘制，并按窗口 DPI 缩放。
- 与原版一样，列表预览字号来自打开窗口时的字号；编辑字号不会立即重建
  列表项的预览字号。字号保存仍限定 3–200，支持小数。
- 选中框维持约28 DIP的固定高度，与原版固定高度控件一致；大字号可能被裁切。
- 保存写入所选显示名称（内置字体含 `#`），取消不改配置。

实现：`tools/FontChooser.{h,cpp}`、`tools/InputSettings.cpp`。
短时 ARM64 设置程序供 ARM64、x64 与 x86 TSF 安装共同使用，无常驻字体服务。

验证：`tests/input_settings_test.py` 检查已保存字号、字体顺序、去重、所有列表
别名匹配、缺失字体回退，以及原有保存/取消/并发字段保留检查。实际控件在独立
且未切换为输入桌面的 Windows desktop 上绘制，生成
`build/font-settings-{96,144,192}.png` 和 `build/font-dropdown-{96,144,192}.png`。
当前字体集合250项；截图确认各列表项按自己的字体显示。

## 设置窗口原生样式

设置窗口保留四页与紧凑双列布局，顶部改为原生 Tab 控件，支持标签上的
方向键，以及 Ctrl+Tab / Ctrl+Shift+Tab 切页。界面字体指定等线（DengXian），
候选字体列表仍使用每个字体自身预览。使用旧版经典控件风格，将内容放入按 TabCtrl_AdjustRect 布局的子容器，使用暖白页面与米色窗口，
输入框保留白底，使用经典控件自身的边框区分，不额外叠加分组框。

回归使用独立 `tools/InputSettingsProbe.vcxproj`，直接调用生产设置窗口，
不再依赖已移除的方案管理测试命令。构建 Release ARM64 后，将
`build/tests/ARM64/input_settings_probe.exe` 复制到
`build/ARM64X/ARM64EC/Release/`（复用旁边的内置字体），运行
`python3 tests/input_settings_test.py`。检查原生标签通知、三档 DPI 的控件范围、
字体预览、保存/取消、参数校验及并发字段保留；输入页截图为
`build/input-settings-{96,144,192}.png`。

选项名称及输入控件使用 16 DIP 等线；辅助说明和底部保存提示使用 13 DIP
等线、深灰色。高对比度模式使用系统文字色；保存失败信息恢复正常字号与文字色。

## 虎爪暖色配色

窗口 #F6F2EA、内容区 #FFFDF8，选项文字 #3A2A20、说明 #7D6B5D。
原生标签控件仅自绘标签项：当前页 #F2C79D，底部 #D96A1B 标记，
保留原生键盘导航、标题与焦点框。输入框和经典按钮、复选框继续使用原生边框。
高对比度模式回退系统背景、文字和选中颜色，响应系统主题/颜色设置变化。
本次颜色调整已构建并验证，尚未更新本机安装或分发安装包。

“赞赏”为第五个标签页，显示内嵌的原始 PNG，通过 Windows WIC 解码并按原比例显示，
不裁切二维码；Ctrl+Tab 循环覆盖全部五页。三档 DPI 截图为
`build/donation-settings-{96,144,192}.png`。
