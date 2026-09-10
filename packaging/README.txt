虎娘 x64 安装包（含 x86 程序支持）

目标：Intel/AMD x64 Windows 10 1809（含 LTSC 2019） 及更新版本。不适用于 ARM64 或 32 位 Windows。

完整解压后双击 install.cmd。允许 UAC 提权；码表初始化仍由原来的 Windows 用户执行。
同名同内容文件自动跳过；内容不同时集中选择覆盖或跳过。覆盖的原文件保存在用户目录 backups 中，files.json 记录对应关系。
安装完成请重新打开需要输入的程序。首次使用的其他 Windows 用户在下一次桌面登录时初始化。

用户数据：%LOCALAPPDATA%\Tigirl
  码表\每个方案一个文件夹；可放 .txt 或 .dict.yaml 码表及 .注释、.拆分文件。
  拼音反查码表\放拼音 .txt 文件；重新加载时编入方案。
  config.txt：输入设置、外观和当前方案。
  自定义选重键.txt：选重键。
  schemas、user：编译结果及用户词条，不要只为更新源码表而删除。
  cache：内部可重建缓存。

任务栏或候选窗右键选择方案、重新加载、打开码表文件夹、输入设置。
没有方案管理窗口，也不需要配置码表位置。
默认包仅含虎码字词、虎整句。个人配置和词条不随包发布。
不读取旧版自定义码表路径，不执行自动迁移。

程序：%ProgramFiles%\Tigirl\versions\版本标识
安装记录：%ProgramFiles%\Tigirl\install.json
程序数据及字体在不同架构目录之间通过硬链接共享。
UWP 的设置和词条操作需要 Tigirl 用户数据目录的 AppContainer 修改权限及低完整性标签；安装器会设置。
UWP 菜单通过 nativetiger URI 启动按需工具；Windows 可能显示打开应用的确认提示。没有常驻 Core。

卸载：Windows“已安装的应用”中选择虎娘，或运行安装目录的 uninstall.ps1。
卸载保留用户数据和程序版本文件，便于恢复；正在运行的程序需要重启。
回滚：以 PowerShell 运行安装目录的 rollback.ps1，可恢复上一程序版本，保留用户数据。

当前发布验证必须区分：在 ARM64 主机上的 x64/x86 仿真测试，不替代 Intel/AMD 电脑上的安装、UWP 和多用户实测。
本包未进行代码签名，Windows 可能显示发布者未知。

品牌名称为“虎娘 / Tigirl”。程序及用户数据目录统一使用 Tigirl，不读取、迁移或兼容旧 NativeTiger/SampleIME 目录。
