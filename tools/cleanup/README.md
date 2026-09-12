# 旧版安装清理

分发仓库根目录的 `Tigirl-Cleanup.bat` 一个文件即可，无需 Python 或额外 PS1。
支持 Windows 10 1809 及以上的 x64、ARM64，以及系统中的 x86 安装组件。

1. 保存 BAT 到本地，双击运行，在 UAC 窗口允许管理员权限。
2. 输入 `CLEAN` 确认清理；直接回车取消。
3. 完成后重启 Windows，再安装新版虎娘。

默认保留所有用户的 `%LOCALAPPDATA%\Tigirl`、`%LOCALAPPDATA%\NativeTiger`
码表、反查表及设置。不终止 QQ、Word、资源管理器等进程，不自动重启。
被占用文件通过 Windows 的重启删除队列清理；在重启之前不要重新安装。
输出失败项和日志位置，不会把部分失败报告为成功。

范围：Program Files / Program Files (x86) 下的 Tigirl、NativeTiger 完整程序目录，
两个注册表视图中的固定 TSF/COM 标识、Active Setup、Tigirl/NativeTiger 卸载入口，
以及指向这些目录的管理协议和开始菜单快捷方式。已登录用户的 TIP 和 SortOrder
缓存按精确 CLSID 清理。未登录用户的注册表配置单元不加载；机器注册清理对全机生效，
其个人设置文件仍保留。非标准目录、其他产品或早期通用 SampleIME 目录不作猜测删除。
不依赖旧卸载程序、DLL 可加载性或完整的 manifest/install.json。

联接点、符号链接及包含它们的程序目录会保留并报告，避免误删目标目录。
固定程序目录内的自放文件也会被删除，请勿把个人文件存入程序安装目录。

`Tigirl-Cleanup.bat /check` 仅列出可发现目标，不提权、不清理。
日志在运行账户的临时目录 `Tigirl-Cleanup-*.log`；使用其他管理员账户提权时，
日志位于该管理员账户下。退出码：0 完成，2 取消，3010 有重启删除项，1 未完成。

维护：修改 `tools/cleanup/cleanup.ps1` 后运行 `python tools/cleanup/build_cleanup.py`，
更新单文件 BAT。不要直接编辑生成的 BAT。
