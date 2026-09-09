# 虎娘图形安装程序

发布物为 `Tigirl-2026.9.10.2-x64-Setup.exe`，单文件、离线，适用 Intel/AMD x64 Windows 10 2004 及以上，包含 x86 程序支持。ARM64 和 32 位 Windows 被拒绝。

安装使用中文 Inno Setup 向导。程序位于 `%ProgramFiles%\Tigirl`，既有受管 NativeTiger 包沿用原程序目录；用户数据固定 `%LOCALAPPDATA%\NativeTiger`。已有配置和个人词条保留，同名源码表可选择跳过或覆盖，覆盖前备份。正式包只含默认虎码字词、虎整句及拼音反查数据。

Windows“已安装的应用”中的虎娘卸载入口启动维护界面。默认保留数据，可勾选删除发起卸载账号的数据（包含备份），再次确认后执行。管理员模式无法可靠保留原用户身份时，该选项禁用。直接运行 `unins000.exe` 始终保留用户数据。其他账号和旧外部源码表目录不被删除。占用的程序文件延迟到重启清理，不强制关闭应用。

## 构建

安装 Inno Setup **6.7.3**，使用其 `ISCC.exe`。`build_setup.ps1` 校验编译器与 ISCmplr.dll 的固定 SHA-256。Windows 自带 .NET Framework 编译器构建按需维护程序，不引入额外下载依赖。

```powershell
.\build_setup.ps1 -Compiler 'C:\path\Inno Setup 6\ISCC.exe'
# 已完成 x64 / Win32 编译时：
.\build_setup.ps1 -SkipBuild -Compiler 'C:\path\Inno Setup 6\ISCC.exe'
# 仅调整 Inno 向导，复用已校验的同版本 payload：
.\build_setup.ps1 -RecompileOnly -Compiler 'C:\path\Inno Setup 6\ISCC.exe'
```

输出 EXE、`.sha256` 和 `.build.json`，位于 `build/packages`。原 ZIP 包与图形包使用独立 staging，避免混用卸载生命周期。已经存在的 staging 不自动覆盖；开发中重新打包应先归档该版本 staging 或使用新版本号。发布前保持 DLL、工具、维护程序和包的版本一致。首版未代码签名。

中文翻译固定取自 Inno Setup `is-6_7_3` 的 `Files/Languages/Unofficial/ChineseSimplified.isl`，保留源文件作者说明。编译器下载来源：https://jrsoftware.org/isdl.php。

## 部署事务与身份边界

Inno 负责解压、跟踪程序文件及底层卸载器；机器后端仅接受固定受管根目录，处理校验、COM/TSF、协议、Active Setup 和安装记录。不接受用户数据目录参数。普通用户初始化通过 `ExecAsOriginalUser` 执行；主动以管理员身份启动时暂缓初始化，由后续普通用户登录完成。

机器后端阶段为 Preflight/Begin、Apply、Commit、Recover、UninstallCheck、Uninstall。事务记录写入安装根的 `setup-transaction.json`，标记注册切换前状态；下一次安装先恢复中断事务。`install.json` 保存当前、上一版本及事务 ID。注册归属不符会停止操作。GUI 包不安装旧脚本包的安装/卸载入口。

用户初始化在原用户目录持有独占锁，`.setup-user-transaction.json` 保存配置和方案指针快照，`backups/install-*/files.json` 在每次覆盖前持久化恢复信息。成功提交后移除事务标记；中断后根据机器提交 ID 决定保留结果或恢复。覆盖的备份继续保留。Quiet 登录初始化跳过内容冲突，不弹批量冲突窗口。

新版本使用独立目录，不覆盖加载中的 DLL；同版本修复补齐缺失文件，不覆盖现存不可变文件。已存在文件的内容被改动时拒绝覆盖，并提示先卸载再重装。保留上一版本用于恢复，更早的受管版本提交后清理。

日志：Inno 默认安装日志，以及程序根 `setup.log`、用户数据目录 `setup-initialize.log`；向导提供打开日志按钮。失败页明确显示未完成和恢复结果；首次安装失败可能留下未注册的 staging 程序文件，后续成功安装/卸载会按清单清理。

## 验收

自动检查：PowerShell 解析、码表冲突合并与回滚、持久用户事务、混合 x64/x86 注册恢复模型、外部注册归属保护、完整包哈希及运行时初始化、ARM64 拦截、图形布局预览。

发布前必须在真实 Intel/AMD x64 Windows 验收：全新安装；旧 ZIP 升级；同版本修复；降级拒绝；UAC 取消和另一管理员账号授权；第二用户首次登录；开始菜单/UWP/Word/x86 输入；取消码表冲突与编译失败恢复；占用 DLL 卸载及重启清理；保留数据/删除当前用户数据；卸载后重装。ARM64 本机上的 x64 仿真运行与隔离测试不能替代这些机器注册场景。
