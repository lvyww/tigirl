# 安装末尾停留在“检查码表冲突并编译缓存”

该文字涵盖 initialize.ps1 的全部当前用户初始化阶段，并不表示一定卡在编译。
安装器先提交机器注册，再运行用户初始化。因此强制结束安装器后仍能使用输入法，
不能证明用户初始化已经完成，也不能据此判断具体阻塞点。

## 已修复：隐藏的码表冲突对话框阻塞升级

图形安装器使用 `ExecAsOriginalUser(..., SW_HIDE, ewWaitUntilTerminated, ...)` 同步等待
`initialize.ps1`。这个调用原本传入 `-NoDialogs`，但初始化脚本仍会在发现同名且内容不同的
码表时调用 `Select-DataConflicts`，由隐藏 PowerShell 进程创建一个独立 WinForms 模态窗口。
升级时更容易出现已有同名文件，因此该窗口若被其他窗口遮挡、落到其他显示器或焦点异常，
主安装器会一直等待。此时 Inno 安装页本身处于安装中的受限交互状态，点击会听到 Windows
提示音，看起来就像卡死在“正在检查码表冲突并编译缓存”。

现在 `-NoDialogs` 是硬性的非交互契约：不会进入冲突选择窗口。图形安装器还显式传入
`-SkipConflicts` 作为第二层保护。冲突项本来的安全默认值就是 `Skip`，因此升级安装时保留
用户现有同名文件，新文件仍正常复制，随后继续编译缓存。日志会记录
`Conflict dialogs suppressed; retaining existing files: conflicts=N`，不会再等待隐藏对话框。

需要手工决定覆盖/跳过时，应在可见的交互式初始化或后续管理界面中完成，不再让隐藏的
安装关键路径依赖另一个进程的无 owner 模态窗口。

## 已修复：等待整个后代进程树

另一个已经修正的等待范围问题是 PowerShell `Start-Process -Wait` 会等待整个后代进程树。
初始化现在使用 `System.Diagnostics.Process` 的句柄等待和退出码检查，只等待
`Tigirl.exe --initialize`；该程序本身同步等待 `Tigirl.Import.exe` 完成后才返回。
没有增加固定延时、超时强杀或跳过编译。

Windows PowerShell 5.1 受控回归 `tests/initialize_process_test.ps1` 使用一个启动
6 秒子进程后立即退出的测试程序：旧等待耗时 7316ms，新等待 360ms，返回时
子进程仍存活。另验证失败退出码 7 能原样返回。这证明等待语义差异，尚不能证明
报告卡住的用户机器确实存在此类后代进程。

## 定位剩余问题

`%LOCALAPPDATA%\Tigirl\setup-initialize.log` 按时间、PID、事务号记录：扫描冲突、
冲突处理策略、复制数据、设置 AppContainer 权限、编译进程 PID/退出码、启用输入法和
写入完成标记。日志写入失败不改变安装和回滚结果。

如果仍复现，保留卡住时的上述日志，以及安装器“打开日志”提供的机器部署日志
（`%PROGRAMDATA%\Tigirl\Logs`）。若日志已经出现 `Compiler started` 而长期没有
`Compiler exited`，再按真正的编译进程阻塞调查；不要仅凭安装页状态文字判断为编译死锁。
