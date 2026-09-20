# x64 Windows 安装包

范围：Intel/AMD x64 Windows，包内包含 x64 和 x86 TSF DLL；不制作 32 位 Windows 独立包。暂定最低系统 Windows 10 1809。ARM64 开发机上的仿真验证不等同于目标系统验收。

## 构建

在 Windows PowerShell 中运行 `./package_x64.ps1`。默认使用同级参考发行目录的模型、虎码字词、虎整句和拼音数据；可通过 `-DataSource` 指定完整发行素材目录。工具链为 VS C++ v145 与 Windows 10 SDK。

输出为 `build/packages/Tigirl-<四段版本>-x64.zip` 及 SHA256。打包只包含两个默认方案，不复制个人配置、用户 journal、测试方案或开发机安装记录。构建所用原始数据不写回参考目录。

`build_x64.ps1` 构建两种 DLL、x64 按需工具和两种加载验证器；工具静态链接 CRT。`package_x64.ps1` 编译内置兜底词库和两个整句 sidecar，包含字体许可证及模型来源记录。每次发布使用新的版本号，避免覆盖已有产物。

## 安装与注册

解压双击 `install.cmd`。64 位 PowerShell 完成检查并启动 UAC 子进程，仅子进程执行机器安装。父进程保留原使用者身份，完成其数据初始化和 `InstallLayoutOrTip` 启用。其他账号使用 Active Setup 在桌面登录时初始化。

机器文件位于 `%ProgramFiles%/Tigirl/versions/<manifest哈希前缀>`，安装记录位于 `%ProgramFiles%/Tigirl/install.json`。x64 使用 System32/regsvr32，x86 使用 SysWOW64/regsvr32，两者共用原 CLSID 和 TSF Profile。`x64` 与 `x86` 目录只保存各自的 TSF DLL；模型、基础词库、字体和桌面工具统一位于同版本的 `shared` 目录，两个架构直接读取同一份资源。

安装器拒绝未知来源的同 CLSID 注册及协议冲突。机器安装失败恢复之前的注册；用户初始化失败恢复数据后尝试回滚机器注册，此时 Windows 可能再次要求 UAC。卸载先删除 x86 COM 注册，再由 x64 DLL 注销共享 Profile，保留用户数据和程序版本目录。`rollback.ps1` 切换到上一程序版本，不回退个人词条。

## 数据与方案

用户根目录固定为 `%LOCALAPPDATA%/Tigirl`。`码表/<方案名>` 中的 txt、dict.yaml 是源码表，`拼音反查码表` 是共享拼音来源。拼音在编译时编入每个方案的 tcd。`schemas` 是内部不可变版本和用户 journal；`cache/sentence` 是整句派生缓存。

独立方案管理窗口已移除；`Tigirl.exe` 仅负责输入设置和按需操作（旧名 `schema_manager.exe`）。菜单枚举源码表直属有效目录，切换、重载、最近方案切换自动调用导入器 `--ensure`。新增文件夹无需手动导入；删除当前目录后回退可用方案。没有任何源码表时保留内置虎码字词兜底。

旧“码表存储位置”“拼音反查目录”不再读取，也不执行自动迁移。测试环境变量 `NATIVE_TIGER_USER_ROOT` 仍供隔离测试使用，带此覆盖不能执行正式用户启用。

相同文件自动跳过；内容冲突集中选择覆盖或跳过。覆盖前保存备份和映射清单。配置、选重键、用户词条不参与默认文件覆盖。首次安装的默认配置关闭输入法自己的 Ctrl+Space 处理，保留系统切换。失败恢复源文件及方案指针；保留生成的新不可变缓存供诊断。

## UWP

用户数据沿用 AppContainer Modify 权限及低完整性标签。菜单主题仍直接写配置；需要桌面工具的操作通过 Windows.System.Launcher 的 `nativetiger://<动作>/<UTF16十六进制方案名>` 激活，操作名和方案名均受验证，不接受任意执行路径。没有常驻 Core，也没有逐键 IPC。Windows 可能显示外部应用启动确认。

## 验证

- `tests/package_data_test.ps1`：冲突分类、默认跳过、覆盖备份、回滚、并发修改保护。
- `tests/package_runtime_test.ps1 -Package <解压目录>`：两个架构加载、隔离用户初始化、默认方案编译、缓存复用、拼音变更、方案增删与最近切换、旧自定义路径不再生效。
- `tests/ManagementUriProbe.vcxproj`：两种架构上的 URI 编解码、Unicode、畸形输入拒绝。
- `python tests/code_mask_tsf_test.py --release-x64`：普通 x64 和 x86 DLL 的真实 TSF 编辑会话，含预编辑、退格、候选、提交、混合输入和取消。

旧 `schema_manager_test.py`、`schema_folder_test.py` 等针对已移除管理窗口的测试不再作为发布验收入口，上述运行时测试替代这些窗口行为检查；底层导入器及版本/journal 维护接口保留。

尚需目标机人工验收：干净 x64 Windows 安装/UAC、微信和 Word、x86 程序、开始菜单和 UWP URI 激活、冲突对话框、多用户、升级/取消/回滚/卸载。安装包未签名。

Windows URI API 依据：https://learn.microsoft.com/en-us/windows/apps/develop/launch/launch-default-app

品牌名称为“虎娘 / Tigirl”。程序及用户数据目录统一使用 Tigirl，不读取、迁移或兼容旧 NativeTiger/SampleIME 目录。
