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
