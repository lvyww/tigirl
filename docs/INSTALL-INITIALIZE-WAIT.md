# 安装末尾停留在“检查码表冲突并编译缓存”

该文字涵盖 initialize.ps1 的全部当前用户初始化阶段，并不表示一定卡在编译。
安装器先提交机器注册，再运行用户初始化。因此强制结束安装器后仍能使用输入法，
不能证明用户初始化已经完成，也不能据此判断具体阻塞点。

已修正的等待范围问题：PowerShell Start-Process -Wait 等待整个后代进程树。
初始化现在使用 System.Diagnostics.Process 的句柄等待和退出码检查，只等待
Tigirl.exe --initialize；该程序本身同步等待 Tigirl.Import.exe 完成后才返回。
没有增加固定延时、超时强杀或跳过编译。

Windows PowerShell 5.1 受控回归 tests/initialize_process_test.ps1 使用一个启动
6 秒子进程后立即退出的测试程序：旧等待耗时 7316ms，新等待 360ms，返回时
子进程仍存活。另验证失败退出码 7 能原样返回。这证明等待语义差异，尚不能证明
报告卡住的用户机器确实存在此类后代进程。

新版 %LOCALAPPDATA%\Tigirl\setup-initialize.log 按时间、PID、事务号记录：
扫描冲突、选择冲突处理、复制数据、设置 AppContainer 权限、编译进程 PID/退出码、
启用输入法和写入完成标记。日志写入失败不改变安装和回滚结果。

若仍复现，保留卡住时的上述日志，以及安装器“打开日志”提供的机器部署日志
（%PROGRAMDATA%\Tigirl\Logs）。旧安装器的 setup-initialize.log 仅记录异常，
等待未结束时可能没有该文件。需结合卡住时的进程状态定位，不应将截图视为编译死锁证据。
