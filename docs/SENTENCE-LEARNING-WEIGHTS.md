# 自学习权重与可读记录

权威说明见 [Tab 纠正自学习](../TAB_LEARNING.md)。首次及后续每次人工纠正均按分差增加 1～3 级，总等级上限 10，每级分差步长为 2，无时间衰减。补充语料中的合法片段也参与已知子串强化。

排序等级与实际确认次数分离，单次跳级不虚增提前上屏成熟度。存储为 `自学习-虎娘.txt`，不再读取旧 `.tigirl-learning.tsv` 或旧日志，不做迁移。

`python tests/sentence_learning_test.py` 包含真实会话中的补充片段发现、加权重放与规划一致性、文件持久化和并发测试；`python tests/test_learning_maintenance.py` 验证可读文件维护。Windows MSVC 可使用学习测试的 `--cxx cl`。
