# 自适应自学习迭代：验证记录

开发基于 GitHub 主线，在 mec-fedora 独立目录编译和测试。zenbook 离线；未改其工作区，未覆盖虎娘候选窗/焦点调整的未提交修改，未打包或安装日用版本。

| 验证 | 结果 |
| --- | --- |
| TigerClaw C# 生产代码及测试项目编译 | 通过，.NET 10.0.401，PublishAot=false |
| TigerClaw 可移植学习套件 | 29,762 项；另包含加权增量索引和可读日志缓存审查 |
| Tigirl C++ 学习套件 | 30,059 项，含真实 SentenceSession 补充子串识别、加权规划、持久化 |
| 两端独立进程日志竞争 | 每端16进程、320个事件、重复重放不加计 |
| Rime Lua 5.4 全套回归及功能负向对照 | 通过；学习23,063项，新策略/真实文本IO 788项 |
| TigerClaw 内附 Rime 全套回归及功能负向对照 | 通过；仅同步学习模块与补充语料接口，未替换独立纠错代码 |
| 新格式 Python 维护工具 | 每端15项，覆盖跳级撤销、重放、备份和损坏文件保护 |
| Git diff whitespace check | 通过 |

新增覆盖：首次/后续每次1..3级、重叠片段不累加、一次片段纠正一条记录、真实确认次数与排序等级分离、补充语料无历史仍可识别、低权重语料精确成员查询、非法编码边界拒绝、加权计划与独立重放一致、UTF-8中文文件/BOM/CRLF/无末行换行/重启/失败写入/旧格式拒绝。

Rime真实文本IO测试使用生产读写代码，互斥锁API由测试对象提供；不代表实际librime宿主验收。C#可移植套件不包含Windows Engine/TSF/Hook协议验收。C++测试在Linux构建；本轮不宣称Windows DLL或安装包已经重建，也不宣称真实输入法前端验收完成。

复现命令：

```console
# TigerClaw
 dotnet run --project next/TigerClaw.Core.Tests -p:PublishAot=false -- --learning-portable-tests
 python3 tests/test_learning_maintenance.py
 python3 tools/run_regressions.py --lua lua5.4 --negative-control
# Tigirl
 python3 tests/sentence_learning_test.py
 python3 tests/test_learning_maintenance.py
# Standalone Rime
 python3 tools/run_regressions.py --lua lua5.4 --negative-control
```

开发机使用系统Lua5.4共享库的独立进程启动器执行相同Lua文件，未为此安装系统软件。正式Windows及其它Lua版本继续由仓库现有CI验证，不在以上已完成结果中。

## CI 比较基线修复（2026-10-04）

旧固定基线 `4f88a4eeffd07fa0cf26e5e26d04d2b760ab6c5b`（作者时间
2026-09-14 00:59:06 +08:00，提交“同步自学习对数权重规则，提高纠正奖励上限”）
仍使用对数权重、时间衰减及至少三次/两个上下文才泛化的规则。
它不是本 PR 的改动前主线，不能要求现在的跨上下文分数与它相等。

本 PR 基线为 `64e07759b1e110d0d88911a6642eed26c362e571`（2026-10-03
11:07:25 +08:00，`feat: seed first correction from score gap`）。其
`SentenceLearningSnapshot::generalScore` 已经是 `level ? 4 + 2 * level : 0`，
第一级跨上下文奖励为 6。这次修复另将同一个独立 C++ 小探针分别编译到旧基线、
该主线和 PR：仅一条空前文的 `aabb / 乙国` 学习记录，查询前文“甲”，结果为
**0 / 6 / 6**。因此 0→6 在本 PR 之前已存在，不是本轮引入的回归。

本 PR 原始提交 `cadbfd7b1aa78a388eaf1efc5287b3c89a816e66` 相对该主线的预期
变化是：后续人工纠正也可按需增加 1～3 级、单事件保存等级、实际确认次数分离、
补充语料片段识别、可读文本持久化及对应维护工具；没有改变第一级泛化奖励。
CI 现在固定比较该改动前主线，使用同一探针独立编译旧版/新版，不跳过或归一化
学习字段。学习/无学习、映射测试模型/无模型共 **5,760** 个快照在 mec-fedora
逐字段精确相同（包括十六进制分数、次序、合法边界、学习模式和标记）。

新行为由 `sentence_learning_test.py` 的 30,059 项专用检查验证，包括首次/后续
跳级、10 级上限、确认次数、补充子串、重叠和持久化；旧行为等价不作为新策略验收。
`sentence_review_test.py` 的 173,044 项检查和全部 11 个功能负向对照也在本地通过。
原有 sanitizer 和 Windows CI 任务保留。以上是本地证据；最终远端 CI 以 PR
最新提交的工作流结果为准，不把阶段提交的通过结果当成最终提交通过。
