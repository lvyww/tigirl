# 虎纹奶油设置窗口交付与验证

完整窗口固定为 640×480 DIP，五页共用虎耳、虎纹标签和暖奶油外框。
透明素材由内置 image_gen 根据选定参考稿生成；文字、控件与候选预览实时绘制。
仅设置程序应用此皮肤，配置键、保存接口、真实候选窗和独立选重键窗口不变。

## 交付文件

- 绘制模块：`tools/SettingsSkin.h`、`tools/SettingsSkin.cpp`。
- 原生窗口接入：`tools/InputSettings.cpp`，两个项目文件及 `SchemaManager.rc`。
- 原始分辨率 PNG、生成提示词：`assets/settings/`。
- 使用说明：`docs/FONT_SETTINGS.md`、`docs/虎娘用户使用说明书.md` 与同名 HTML。
- 全部实屏截图与机器验证结果：`design/settings-640x480/tiger-cream-implemented/`。

## 五页截图（150%）

| 输入行为 | 候选外观 | 按键与快捷键 | 整句输入 | 赞赏 |
| --- | --- | --- | --- | --- |
| [截图](../design/settings-640x480/tiger-cream-implemented/screen-0-144.png) | [截图](../design/settings-640x480/tiger-cream-implemented/screen-1-144.png) | [截图](../design/settings-640x480/tiger-cream-implemented/screen-2-144.png) | [截图](../design/settings-640x480/tiger-cream-implemented/screen-3-144.png) | [截图](../design/settings-640x480/tiger-cream-implemented/screen-4-144.png) |

同目录保留 100%、125%、200% 下的全部五页，以及四档错误提示、极端预览、
隐藏候选、高对比度分支截图。截图直接读取屏幕像素，包含完整虎耳外框；
外轮廓之外可能显示其后的桌面。另有隔离桌面的真实控件 WM_PRINT 回归截图在 `build/`。

## 已通过

- Release ARM64、x64、Win32 编译，警告视为错误。
- 四档固定外接尺寸：640×480、800×600、960×720、1280×960 像素。
- 五页可达，Tab/Shift+Tab、Ctrl+Tab/Ctrl+Shift+Tab、Space 勾选、原生下拉框、快捷键设置。
- 标题命中与实际拖动、系统键盘移动、Alt+Space 菜单、最小化/恢复、关闭不保存。
- 范围校验、错误切页/滚动/聚焦、依赖控件禁用与恢复、快捷键冲突与保存后的引擎分派。
- 字号显示一位小数并保留未编辑的原始精度，未知键、注释、并发修改均保留；无修改不写文件，取消不保存。
- 200 DIP 字号、横排 10 候选及隐藏候选预览；预览不修改配置。
- 四个设置页滚动后与完整重绘比较均为 **0 像素差异**；按真实窗口区域排除外部桌面。
- 重复开关窗口、切页和四档 DPI 切换，GDI/USER 句柄计数稳定。

执行入口：

```sh
python3 tests/input_settings_test.py
python3 tests/settings_skin_test.py --active-desktop
python3 tests/input_settings_scroll_test.py --active-desktop
```

详细断言、二进制哈希与安装结果见
[validation.json](../design/settings-640x480/tiger-cream-implemented/validation.json)。

## 本机更新

沿用已有暂存、预检、版本目录和回退记录流程，只替换 `Tigirl.exe`。
首次接入版本目录为 `e3675c81277fccec`；安装后在系统 150% 缩放下验证 960×720
像素、五个标签、预览控件、入口快捷方式和 URI，并打开新版窗口。
输入引擎、字典、模型、字体及用户配置均核对哈希不变，没有覆盖未保存编辑。
没有生成或发布分发包。已有候选窗与焦点相关未提交源码改动保持完整。

设置程序 SHA-256：`C4B9D0AA93A7FCF7A4F18D1F1579C8689F1B879F4CB2839CD418101DB2867EF4`。
回退记录：`build/tiger-cream-install-20261004/build/previous-install-*.json`；
更新前记录与配置快照留在同级 `before/`。

## 验证边界

四档 DPI 通过生产缩放函数驱动，未实施实体多显示器间的 DPI 迁移。
高对比度已验证系统配色分支及装饰隐藏，未切换 Windows 全局高对比度设置。

## 黑体与边距修订

设置界面字体改为黑体（SimHei），候选预览继续读取用户配置的候选字体。
关闭按钮左移 24 DIP；底栏按钮上移 8 DIP、收窄并增加右侧留白，
修改状态提示上移 9 DIP、缩短背景区域，避免遮盖内框。外接尺寸仍为 640×480 DIP。
四档 DPI 五页实屏截图已刷新，窗口交互与句柄回归通过。

已更新本机至 `60f14370499b2aec`，打开新版窗口；引擎、字典与用户配置保持不变。
修订验证见 [inset-validation.json](../design/settings-640x480/tiger-cream-implemented/inset-validation.json)。
回退记录位于 `build/tiger-cream-inset-install/`。

## 中间页面底板修订

移除中间不透明圆角底板及边线，页面与小虎娘背景共用父窗口奶油底色，
两侧虎纹不再被底板盖住。完整尾巴缩放到固定底栏，避免在页面边缘截断。
四档 DPI 五页截图和原生窗口交互检查通过；滚动与完整重绘比较仍为 0 像素差异。
当前截图已刷新，结果见 [transparent-validation.json](../design/settings-640x480/tiger-cream-implemented/transparent-validation.json)。

本次本机替换未执行：Windows 提权请求被取消，安装记录保持 `60f14370499b2aec`。
新版已构建，待安装文件及回退准备位于 `build/tiger-cream-transparent-install/`。
没有重试提权，也没有改动现有配置。


## 简化设置与文本框居中（2026-10-05）

- 设置程序使用黑体及共用奶油背景，保留标题栏、底栏内边距。
- 移除 Ctrl+空格设置及引擎/TSF 切换处理。旧配置中即使保留“是”也不会重新启用；该组合键交给 Windows。
- 固定预览为 244×148 DIP、14 DIP 字号和三个示例候选；保留字体、主题、排列示例，移除额外外框及缩放说明。实际候选字号和数量仍正常保存。
- 页面补充说明改为对应项的原生悬停提示；实际鼠标悬停验证通过。
- 设置控件取消 Tab stop 和虚线焦点框，输入框仍可点击后编辑。文本框与快捷键文字水平、垂直居中；例外字符超高时滚动。
- 五页 × 100%、125%、150%、200% 实际屏幕截图已更新；采集新增遮挡检查，避免将其他窗口覆盖的像素作为验证依据。
- 四页增量滚动与完整重绘为零像素差异。保存、取消、原字号精度、未知键/注释、并发字段保留、关联禁用与恢复、快捷键冲突、长多行例外字符测试通过。
- Ctrl+空格专用回归包含 16 组旧配置/中英文/编辑状态/松键顺序组合，覆盖长按重复；均不切换、不吞键、不更改编码。
- ARM64、x64、x86 DLL 加载和类实例化通过。设置程序重复开关及 DPI 切换的 GDI/USER 句柄检查通过。

本轮结果见 [simplified-validation.json](../design/settings-640x480/tiger-cream-implemented/simplified-validation.json)。
本机待安装目录为 `build/settings-simplify-install/`，预检通过，仅更新设置程序与输入引擎，其他已安装资源逐项校验保持一致。
此前 Windows 管理员授权被取消，本轮尚未再次提权或修改安装；未发布分发包。

## 独立装饰与下拉列表修正

铃铛、虎尾、爪印使用新增透明图集（资源 15），外框保持原始比例单独绘制。铃铛完整位于 y=14..56 DIP 的可见标题范围，虎尾源矩形不含边框残段；两枚爪印位于底栏空白处，不再被页面子窗口遮挡。图集与源提示词均保留在 `assets/settings/`。

展开的原生 ComboLBox 不设置圆角窗口区域，也不覆盖其非客户区滚动条。列表保持直角、奶油色内容与原生滚动条。`tests/settings_dropdown_test.ps1` 检查弹出列表可见、没有裁角区域，发送滚动消息确认首个可见项发生变化，并捕获实际屏幕图像、验证配置未写入。

## 2026-10-05 本机设置程序更新

安装代次 `9b6c9f0a0c9f4938`，设置程序 SHA-256：
`DFDE1E8369589B7A62A78CEB12B3B0C8BFB2993CA0119C0B55B8ABF6AC78AB3C`。
本机 144 DPI 下实际窗口为 960×720，五页及候选预览存在，开始菜单、URI 和注册入口验证通过；已打开安装目录中的新版设置窗口。

本次仅部署设置程序，输入引擎 DLL、字典及用户配置哈希保持不变，保留上一代安装与回退记录。Ctrl+空格引擎移除代码随源码提交，但不在此次仅设置程序的部署范围内；旧引擎对旧配置的行为不因本次安装改变。原有窗口中的未保存编辑没有被覆盖。

验证：原生设置测试、五页四档 DPI 截图、下拉列表直角/滚动测试通过；Ctrl+空格源码对应测试构建通过 16 组组合回归。详见 `design/settings-640x480/tiger-cream-ornament-fix/validation.json`，本机详细安装及回退记录留在 `build/tiger-cream-final-install/`。
