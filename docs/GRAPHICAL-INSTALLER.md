# 虎娘图形安装程序

发布物为 `虎娘-版本号-x64-ARM64-x86-安装程序.exe`，单文件、离线，适用 Intel/AMD x64 Windows 10 1809 及以上和 ARM64 Windows 11，包含 x86 程序支持。安装器自动选择 x64 或 ARM64X 输入法 DLL，ARM64X 在 ARM64 和仿真 x64 应用中共用一份；桌面工具、码表、皮肤和模型放在 shared / DefaultData 中。32 位 Windows 不支持。

也可构建 `虎娘-版本号-x64-x86-安装程序.exe`，仅包含 x64 / x86 组件，适用于 Intel/AMD 64 位 Windows。两种包均包含完整皮肤、码表和模型，共用初始化与完成页修复。x64 包中的 x86 指 32 位应用支持，不表示支持 32 位 Windows；ARM64 设备使用通用版。

安装使用中文 Inno Setup 向导。程序位于 `%ProgramFiles%\Tigirl`，用户数据固定 `%LOCALAPPDATA%\Tigirl`。已有配置和个人词条保留；图形安装/升级遇到同名且内容不同的源码表时默认保留用户现有文件，新文件仍正常复制。默认包包含虎码字词、虎整句、五笔86、五笔86整句、拼音反查、`resources/Skins` 下的正式皮肤及整句模型。当前正式资源为十二套皮肤（含默认）；雨竹听风、雪夜铃兰、霓虹电玩保留第一版原图，桂月团圆、柚光汽水及幽焰小恶魔均已从方向稿制作成横竖排 SSF。这六款采用 `font_size=140` 和紧凑布局，字边距参照虎娘-虎纹奶油逐款校准。资源调整从下次完整打包起生效，已生成的旧安装包不会自动变化。深海舰长已从当前默认皮肤集移除。

小恶魔系列正式款仅保留幽焰小恶魔，使用 `python3 tools/build_little_devil_ssf.py resources/Skins` 重建。绯绒小恶魔、莓糖小恶魔于 2026-10-18 归档至 `皮肤/归档/20261018-小恶魔`，使用 `python3 tools/build_little_devil_ssf.py --archived` 可重建归档。源图、提示词和两套参数清单保留在 `assets/skins/little-devil-20261017`；历史渲染见 [小恶魔皮肤预览](../design/little-devil-skins-20261017/index.html)。打包时按 `assets/skins/catalog.json` 的 `archived` 清单排除两款及旧名称，防止旧文件误放回资源目录后重新入包。构建原样封装 PNG，不删除已有用户皮肤或修改用户配置。

皮肤原画、制作源素材与成品纳入版本库；历史预览、对照截图、试验版本及报告仅保存在本机。文中历史预览链接指向本地记录，不随源码上传。

Windows“已安装的应用”中的虎娘卸载入口启动维护界面。默认保留数据，可勾选删除发起卸载账号的数据（包含备份），再次确认后执行。管理员模式无法可靠保留原用户身份时，该选项禁用。直接运行 `unins000.exe` 始终保留用户数据。其他账号和旧外部源码表目录不被删除。占用的程序文件延迟到重启清理，不强制关闭应用。

## 一键打包通用安装程序

在项目根目录双击 **`build_setup_universal.cmd`**。它调用实际打包脚本 `build_setup_universal.ps1`，在同一个窗口显示阶段进度、日志路径和错误，结束后按任意键关闭窗口。支持转发参数，例如在命令提示符中运行 `build_setup_universal.cmd -CheckOnly`。

需要静默后台运行时，可双击 `build_setup_universal.vbs`；它也调用同一个 `.ps1`，但隐藏窗口，所以点击后不会显示进度或完成提示，需查看下方状态文件。两个入口都不自动安装生成的 EXE，同一工作区已有打包任务时会拒绝重复启动。

默认流程包含：

1. 从 `SampleIME/SampleIME.rc` 读取版本，校验所有工具资源及维护程序的版本一致；检查 Inno、C++ 工具链和模型。
2. 使用 MSBuild 增量编译 x64、Win32 和 ARM64X，准备共享桌面工具。
3. 从当前工作区重新收集全部码表、拼音反查、SSF 皮肤、字体与模型，生成兜底词库和完整清单。每次创建独立 staging，不复用旧的同版本资源包。
4. 校验所有文件哈希、7 个发布二进制的版本、架构、模型和资源与工作区的一致性；执行本机支持的 DLL 加载检查。
5. 运行安装脚本编译、架构策略、空白用户初始化、缺失资源修复与用户修改保留、完成页交互回归。用户数据测试使用临时目录，完成页测试使用隔离桌面。
6. 在 Windows 临时目录编译单文件安装程序，核对成品与复制后的 SHA-256，最后交付整套文件。

前置条件：64 位 Windows PowerShell 5.1；Visual Studio C++ v145 工具集及 x64/x86、ARM64/ARM64EC 支持和 Windows SDK；Inno Setup **6.7.3**；仓库所需模型 `data/Models/sentence-fivegram-mobile.bin`。模型、皮肤及编译器均执行已有固定哈希/资源检查，脚本不自动下载依赖。默认 Inno 路径为 `build/tools/InnoSetup-6.7.3/ISCC.exe`。

命令行也可使用：

```powershell
# 完整默认流程，版本自动读取，支持同版本重复执行
.\build_setup_universal.ps1
# 只检查依赖、资源和源码版本，不构建
.\build_setup_universal.ps1 -CheckOnly
# 已确认编译产物最新时，跳过 C++ 编译；仍重新收集资源并执行全部打包校验
.\build_setup_universal.ps1 -SkipBuild
# 自定义编译器和交付根目录
.\build_setup_universal.ps1 -Compiler 'D:\Inno Setup 6\ISCC.exe' -OutputDirectory 'D:\Tigirl Releases'
```

版本不再需要在命令中手动填写。若传入 `-Version`，它必须与源码资源版本一致；脚本不会把旧二进制改名成新版本，也不会修改工作区的版本资源。需要升版时先同步 DLL、四个工具的 `.rc` 和 `packaging/setup/Maintenance.cs`。

一键入口交付的文件名包含本机的构建开始日期和时间，例如
`虎娘-20261017-231830-v2026.10.15.1-x64-ARM64-x86-安装程序.exe`。
`20261017-231830` 是这次打包的时间，`v2026.10.15.1` 是程序版本；压缩跨过
午夜仍沿用本次开始时间，与运行目录一致。SHA-256 校验文件、构建元数据和
成功记录同时使用新的文件名。内部编译 staging 的名称仍使用程序版本。

进度与成品位置：

- `build/setup-runs/latest-universal.json`：最近一次构建的状态文件、阶段进度和日志路径；`status.json` 中的 `state` 为 `running`、`passed` 或 `failed`。
- `build/setup-runs/<运行编号>/`：各阶段日志、资源清单、测试报告和本次独立 staging。失败时保留现场，并写入 `error.log`。
- `build/packages/latest-universal.json`：最近一次**成功交付**的安装包路径及 SHA-256；失败不会覆盖这个记录。
- `build/packages/releases/<版本>-universal-<运行编号>/`：EXE、`.sha256`、`.build.json`、`.manifest.json` 和 `verification.json`。自定义 `-OutputDirectory` 时，成品和成功记录位于该目录。

重复运行不会删除旧包或覆盖已有安装程序；同一工作区的此入口同时只允许一个构建。编译输出位于独立的 Windows 临时目录，避免前几次直接写桌面成品目录时遇到文件占用。复制阶段会短暂重试，完整校验成功后才更新成品目录和成功记录。

本机是 ARM64 时，会执行 ARM64 与仿真 x64 两种宿主对 ARM64X 的加载检查；在 x64 构建机上仅执行可运行的 x64/x86 检查。自动构建不会注册输入法，也不替代文末列出的真实机器安装、UWP 和输入应用验收。

## 底层构建入口

安装 Inno Setup **6.7.3**，使用其 `ISCC.exe`。`build_setup.ps1` 校验编译器与 ISCmplr.dll 的固定 SHA-256。Windows 自带 .NET Framework 编译器构建按需维护程序，不引入额外下载依赖。

```powershell
.\build_setup.ps1 -Compiler 'C:\path\Inno Setup 6\ISCC.exe'
# 已完成 x64 / Win32 / ARM64X 编译时：
.\build_setup.ps1 -SkipBuild -Compiler 'C:\path\Inno Setup 6\ISCC.exe'
# 仅调整 Inno 向导，复用已校验的同版本 payload：
.\build_setup.ps1 -RecompileOnly -Compiler 'C:\path\Inno Setup 6\ISCC.exe'
# 另行生成只含 x64 / x86 的安装程序（已有对应编译产物）：
.\build_setup.ps1 -Architecture x64 -SkipBuild
```

底层入口默认输出 EXE、`.sha256` 和 `.build.json` 到 `build/packages`。原 ZIP 包与图形包使用独立 staging，避免混用卸载生命周期。直接运行底层入口时，已经存在的 staging 不自动覆盖；通常应使用上方的一键入口，它会自动分配本次目录。发布前保持 DLL、工具、维护程序和包的版本一致。首版未代码签名。

`-Architecture` 默认 `universal`，也接受 `x64`；staging、安装包文件名、向导说明和架构入口按该参数选择。`-RecompileOnly` 同样需要指定对应架构，且校验 payload 是否含有相应组件，防止把 ARM64 包误标为 x64。

可用 `-OutputDirectory <目录>` 指定 EXE 及校验文件的输出位置，例如先在独立临时目录完成编译，再复制成品，避免正在使用的安装程序占用输出文件。

`-WorkDirectory <目录>` 指定本次 staging 根目录；`-StageOnly` 只构建和校验 GUI payload，供一键入口在压缩前执行测试。GUI 流程不生成无用的中间 ZIP；独立调用 `package_x64.ps1` 的默认 ZIP 行为保留。

中文翻译固定取自 Inno Setup `is-6_7_3` 的 `Files/Languages/Unofficial/ChineseSimplified.isl`，保留源文件作者说明。编译器下载来源：https://jrsoftware.org/isdl.php。

## 部署事务与身份边界

Inno 负责解压、跟踪程序文件及底层卸载器；机器后端仅接受固定受管根目录，处理校验、COM/TSF、协议、Active Setup 和安装记录。不接受用户数据目录参数。普通用户初始化通过 `ExecAsOriginalUser` 执行；管理员令牌属于当前交互账号时允许初始化，其他管理员账号或无法确认身份时暂缓。

机器后端阶段为 Preflight/Begin、Apply、Commit、Recover、UninstallCheck、Uninstall。事务记录写入安装根的 `setup-transaction.json`，标记注册切换前状态；下一次安装先恢复中断事务。`install.json` 保存当前、上一版本及事务 ID。注册归属不符会停止操作。GUI 包不安装旧脚本包的安装/卸载入口。

用户初始化在原用户目录持有独占锁，`.setup-user-transaction.json` 保存配置和方案指针快照，`backups/install-*/files.json` 在每次覆盖前持久化恢复信息。成功提交后移除事务标记；中断后根据机器提交 ID 决定保留结果或恢复。图形安装器的原用户初始化是隐藏并同步等待的，因此同时传入 `-NoDialogs -SkipConflicts`：任何同名内容冲突都沿用 `Get-DataMerge` 的安全默认值 `Skip`，不会创建另一个进程的模态选择窗口。`initialize.ps1` 本身也保证 `-NoDialogs` 永远不调用冲突选择器，防止调用方遗漏 `-SkipConflicts` 后再次出现隐藏窗口阻塞。Quiet 登录初始化同样跳过内容冲突。

完成页另检查窗口原生的启用状态：现场曾出现所有安装/初始化子进程已退出、没有模态弹窗，向导仍保留 `WS_DISABLED`，导致“完成”、勾选框和“打开日志”全部无法点击。只在 `wpFinished` 恢复残留禁用状态，安装和模态等待期间不解除禁用。恢复同时处理 VCL 属性与原生窗口状态，不能只设置可能已经为 True 的 `Enabled` 属性。每个同步后台步骤及完成页都会将窗口启用状态写入 Inno 日志，供后续定位具体触发阶段。

新版本使用独立目录，不覆盖加载中的 DLL；同版本修复补齐缺失文件，不覆盖现存不可变文件。已存在文件的内容被改动时拒绝覆盖，并提示先卸载再重装。保留上一版本用于恢复，更早的受管版本提交后清理。

日志：Inno 默认安装日志，以及程序根 `setup.log`、用户数据目录 `setup-initialize.log`；向导提供打开日志按钮。用户初始化日志会记录冲突数量以及非交互模式保留现有文件的决策。失败页明确显示未完成和恢复结果；首次安装失败可能留下未注册的 staging 程序文件，后续成功安装/卸载会按清单清理。

## 验收

自动检查：PowerShell 解析、非交互升级冲突策略、码表冲突合并与回滚、持久用户事务、混合 x64/x86 注册恢复模型、外部注册归属保护、完整包哈希及运行时初始化、ARM64 安装入口、ARM64 / x64 双视图加载和按架构选择注册路径、图形布局预览。

`tests/setup_build_workflow_test.ps1` 检查版本自动读取与不一致拒绝、构建时间命名与跨日发布、校验文件和成品名称一致、同版本重复运行隔离、成品损坏或清单变化时拒绝交付，以及失败后保留上次成功记录。测试使用临时文件，不编译或安装输入法；真实完整构建由 `build_setup_universal.ps1` 验证。

`tests/setup_finished_page_test.ps1 -Compiler <ISCC.exe>` 编译实际完成页处理函数，在不切换到前台的隔离桌面运行真实 Inno 窗口。移除恢复调用的对照包必须重现禁用问题；修复包覆盖原生/VCL 禁用、正常完成、失败、用户初始化失败、延后和重启清理状态，实际点击日志与完成按钮、切换启动勾选框并检查退出和启动行为。测试只运行无文件安装、无注册表操作的夹具，不重装本机输入法。

发布前必须在真实 Intel/AMD x64 Windows 验收：全新安装；旧 ZIP 升级；同版本修复；降级拒绝；UAC 取消和另一管理员账号授权；第二用户首次登录；开始菜单/UWP/Word/x86 输入；已有同名且内容不同的码表升级时确认安装不弹隐藏冲突窗口且保留用户文件；编译失败恢复；占用 DLL 卸载及重启清理；保留数据/删除当前用户数据；卸载后重装。ARM64 本机上的 x64 仿真运行与隔离测试不能替代这些机器注册场景。

## 首次安装只有兜底词库时

先检查 `%LOCALAPPDATA%\Tigirl\setup-initialize.log`。没有日志表示尚不能确认初始化主体启动；旧版在管理员令牌下提前退出 20 时没有日志，进程未启动、前置脚本加载失败等也需要看 Inno 日志。2026-10-05 首个 SSF 包到 2026-10-14 包的初始化相关脚本哈希一致，当前用户报告的具体触发原因仍需现场验证，不能仅凭缺少日志归因于码表格式。

新增的初始化身份检查允许当前交互会话用户使用自己的提升令牌（包括关闭 UAC 或内置管理员），仍拒绝无法确认身份或另一个管理员账号；检查结果及延后原因写入日志。安装器也记录 UserStep 启动失败或退出码。默认资源必须与清单哈希一致，禁止在缺少资源时仅编译兜底词库却写入成功标记。

受影响用户可在日常账号直接双击 `repair-user-data.cmd`，配套 `repair-user-data.ps1` 必须在同一目录。工具读取本机安装记录、校验已安装资源，使用现有事务复制/编译流程补齐当前用户数据，不重新注册或卸载输入法；即使同版本完成标记已存在也会修复缺失文件。已有配置、用户修改和个人词条继续使用原冲突保护。诊断日志写入 `%LOCALAPPDATA%\Tigirl\setup-repair.log`，原初始化日志保留。安装目录的资源缺失时工具明确报错，需要重新取得完整安装程序。

回归：`tests/setup_first_user_data_test.ps1 -Package <实际安装包 payload>` 检查同账号提升/不同账号身份策略、真实会话 SID、错误的完成标记、全部皮肤和码表、缺失皮肤恢复、用户修改保留、缺失安装资源失败及日志。真实 x64 首次安装仍须由受影响机器复测。

## ARM64 安装包验证

`build_setup.ps1` 默认生成 universal 包，内部调用 `package_x64.ps1 -IncludeArm64` 同时构建 x64、Win32 和 ARM64X。`-SkipBuild` 要求三种架构的编译产物均已准备。安装预检会校验清单全部文件；在 ARM64 上额外由原生 ARM64 和 x64 探针分别加载 ARM64X 并创建 COM 类。注册、恢复及卸载统一按 native / x86 两个注册槽处理，缺失 DLL 时仍由 manifest 决定原有架构路径。

架构策略依据 [Inno Setup ArchitecturesAllowed](https://jrsoftware.org/ishelp/topic_setup_architecturesallowed.htm)；ARM64X 双视图机制见 [Microsoft ARM64X PE files](https://learn.microsoft.com/en-us/windows/arm/arm64x-pe)。构建和隔离验证不向本机注册输入法，保留空白环境给手工首次安装测试。

`tests/setup_package_activation_test.ps1` 使用独立 ARM64 / x64 / x86 宿主加载实际打包 DLL，在临时用户目录启动资源读取。空白系统没有 TIP 注册时，Windows 在后续挂接键盘事件阶段返回 `E_INVALIDARG`；测试只接受这个精确阶段，证明此前的词库和设置读取成功，其他启动错误均失败。这项检查不等同于已注册输入测试，报告单独记录 `activation_completed` 和 `registered_input_tested`。
