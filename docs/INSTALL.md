# 虎娘 ARM64 开发版

当前是开发验收版本，完整体验尚未完成。普通 ARM64 DLL 仅通过 ARM64
宿主加载；ARM64X 包已通过 ARM64 和 x64 宿主加载与 TSF 测试，x86 补充组件已通过 COM 激活检查。
具体证据见 [验收记录](ACCEPTANCE.md)。不要把“Windows 是 ARM64”
理解成当前 DLL 已覆盖系统上所有架构的应用。

ARM64X 开发包已完成实际安装和 ARM64/x64 系统注册激活检查，
回退、卸载及重装闭环也已通过，真实应用输入仍待验收。
使用以下命令构建、预检及安装：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_arm64x.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install_arm64.ps1 -Arm64X -CheckOnly
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install_arm64.ps1 -Arm64X
```

产物位于 `build\ARM64X\ARM64EC\Release`，需要 ARM64EC 工具和库。
构建命令同时构建两种 ABI 的 DLL，并放入 ARM64 版伴随工具及 ARM64/x64
加载验证程序；构建和预检不会更新注册。`-Arm64X` 选择此目录；省略它仍安装
普通 ARM64 包。预检会实际运行两种架构的加载验证，确认 DLL 能创建输入法
COM 对象后，实际安装才请求管理员权限。复制完成后会再次校验目标目录的
文件哈希，并从该目录运行双架构加载检查，通过后才更新注册；检查结果写入
安装记录的 `installed_load_checks`。仅有 ARM64 PE 文件头不足以通过
ARM64X 预检。加载检查不等于在真实应用中完成输入验收。

## 构建与预检

在项目根目录使用原生 ARM64 Windows PowerShell。当前工程使用 Visual Studio
v145 C++ 工具集、ARM64 编译工具和 Windows SDK；构建脚本通过 vswhere 定位 VS。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build_arm64.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install_arm64.ps1 -CheckOnly
```

产物位于 `build\ARM64\Release`，包含 TSF DLL、共享二进制码表、字体，以及
方案管理、导入、选择和一次性计时提醒工具。`-CheckOnly` 检查架构、文件和
哈希并显示拟安装目录，不执行注册。

开发版构建现在还包含 `Models\sentence-ngram-mobile.bin` 和来源记录。
默认从本地 `data\Models\sentence-ngram-mobile.bin` 读取已验证模型（不纳入 Git）。
当前默认采用最初训练模型的 m5 剪裁版：
`trainer_v2/full-kn-m5-v2/sentence-ngram-mobile.bin`（TCSKNM02），无需展开成 TCSKNM01。
将该文件复制到上述本地默认路径即可参与构建。文件大小为 469,886,928 字节（约 448.1 MiB），
SHA256 为 `c0063898fdff27c1fb00c1c72fa28a6c1b375fade1ec2045d731b9db958bdecc`。
TCSKNM02 与同源 TCSKNM01 仅存储布局不同，保留每个 n-gram 及 float32 概率。
其他来源须同步更新 `sentence_package.ps1` 的固定哈希与长度。

其他目录可通过 `build_arm64.ps1` 或 `build_arm64x.ps1` 的
`-SentenceModelPath '完整路径\sentence-ngram-mobile.bin'` 参数指定。
构建和安装预检均核对固定模型的长度与 SHA256，缺失或不匹配时停止。
模型属于非神经整句功能，包内不包含 Qwen。整句选项见
[整句输入设置](SENTENCE_SETTINGS.md)。

## 安装与使用

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install_arm64.ps1
```

脚本会请求管理员权限，将完整包放入 `C:\Program Files\Tigirl\versions`
下由包哈希确定的独立目录，注册“虎娘”，并创建开始菜单
“虎娘 → 方案管理”。安装结果记录在 `build\native-install.json`。
安装后重新启动要使用输入法的应用，让它们加载新一代 DLL。

可从语言栏右键菜单进入“方案管理”或“输入设置”。用户配置默认位于
`%LOCALAPPDATA%\Tigirl`；开发测试可通过 `NATIVE_TIGER_USER_ROOT`
指定独立绝对路径。正常打字在 TSF 进程内执行，不启动原版 Core。
计时命令使用一次性提醒进程，管理和导入使用短时工具进程。

此项目沿用 SampleIME 的配置标识，但与日用虎爪目录
`C:\Users\yc\Desktop\bime_codex_src_20260513\release_arm64` 分开。
当前已注册 ARM64X 代 `5aa4bd69945cfb9c`，包含 DirectWrite / Direct2D 候选窗、
Ctrl+空格修复和目录式方案管理。32 位补充组件也已同步更新。
安装完整性和系统注册检查见 `build/folder-installed-validation.json`。

方案管理现在直接选择码表根目录下的方案文件夹，自动准备二进制缓存。
使用方式见 [目录式方案管理](FOLDER_SCHEMAS.md)。首次使用原虎爪目录时，选择
`C:\Users\yc\Desktop\bime_codex_src_20260513\release_arm64\码表`。
此前用户已确认普通应用输入和新版字体大致正常；当前安装的真实应用及跨屏
验收应与历史自动测试记录区分，不能仅凭注册检查宣称全部通过。

## 回退和卸载

安装新代时，如果已有其他代注册，会保存
`build\previous-install-<新代标识>.json`。回退时使用该快照的实际路径：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\rollback_arm64.ps1 -Snapshot .\build\previous-install-<新代标识>.json -CheckOnly
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\rollback_arm64.ps1 -Snapshot .\build\previous-install-<新代标识>.json
```

回退会核对旧 DLL 的位置和 SHA256 哈希，再恢复注册与管理入口。
快照缺少哈希、哈希格式错误或 DLL 内容不匹配时，会在提权前拒绝回退。它不会让已经运行的
应用自动卸载 DLL，仍需重新启动应用。卸载使用与当前注册代匹配的安装记录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\uninstall_arm64.ps1 -Record .\build\native-install.json -CheckOnly
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\uninstall_arm64.ps1 -Record .\build\native-install.json
```

卸载保留用户数据和二进制版本目录。回退后如果安装记录不再对应当前注册代，
卸载脚本会拒绝操作；不要通过手改哈希绕过检查。此前 `e3b95d4b9183e325` 包已完成安装 → 回退 → 重装 → 卸载 → 重装闭环。
`build\deployment-cycle.json` 记录各阶段核对结果：卸载清除本输入法的 COM、
语言配置和自有快捷方式，用户数据哈希及日用虎爪注册保持一致；最后恢复
当时的 ARM64X 版本，并通过 ARM64/x64 系统注册激活检查。该历史闭环不代表当前包已重跑回退和卸载。


## 32 位程序支持

ARM64X 覆盖 ARM64 和 x64；32 位程序另外使用 x86 DLL。当前已安装的
补充组件记录在 `build/native-install-x86.json`，32 位系统 COM 激活检查通过。
源码的 `Release|Win32` 已更新为 v145，独立输出至 `build/Win32/Release`。
安装使用原生 ARM64 PowerShell：

```powershell
.\install_x86.ps1 -CheckOnly
.\install_x86.ps1
```

脚本要求有效的主安装记录，先验证 x86 加载器及全部主包文件，再申请提权。
32 位 DLL 使用独立版本目录；码表和伴随工具通过硬链接共用主包文件。
主安装升级后应重新构建并更新 x86 补充组件。x86 安装脚本会核验旧代
注册与安装记录、保存回退记录，再将注册切到新的独立目录。不会覆盖已加载的 DLL。
主包新增的模型也通过同一硬链接流程共用，保持相同的文件后端。

完整卸载顺序为 `uninstall_x86.ps1`，再执行 `uninstall_arm64.ps1`；两者均有
`-CheckOnly`。x86 卸载只移除匹配记录的 32 位 COM 注册，保留共享输入法
配置、用户数据和文件。主卸载在 x86 注册仍存在时拒绝删除共享配置。

安装完成后须彻底退出并重开使用输入法的程序。本轮微信实测仍驻留旧代 DLL，
重启后恢复；用户随后确认 Word 和 32 位 Pain 打器也可以正常输入。
Word 恢复的具体原因未单独验证。
