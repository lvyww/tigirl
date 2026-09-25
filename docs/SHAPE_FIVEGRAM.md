# 虎娘 TCSKNM03 Q8 五阶整句主线

虎娘直接使用 TCSKNM03 version 2 Q8 纯汉字五阶模型参与每次 Beam 扩展和 EOS 评分。
孤立字判断也由同一模型的 observed bigram 提供；正式运行时不加载三阶模型，
也不依赖 KenLM、jointkenlm.dll、Qwen 或独立 Core。

## 资源与格式

- 开发源：`data/Models/sentence-fivegram-mobile.bin`（不纳入 Git）。
- 运行文件：`Models/sentence-fivegram-mobile.bin`。
- 格式：`TCSKNM03` version 2，order=5，词表 20,799 项，Q8 概率/backoff。
- 长度：405,663,171 字节（约 386.87 MiB）。
- SHA256：`756f6c92cf43ad6e8e3087ce66b711ac6ad0fc41e6f3fb82b3766e35ecab8681`。
- 原词汇先验 `sentence-lexical-v1.bin` 继续保留。

主线来自 `corpus4-articles-third-20260925/sentence-fivegram-mobile.bin`：
Corpus4 50%、Articles 25%、Brightmart 非新闻 25% 三路概率融合，随后剪枝和 Q8 量化。
一至五阶记录数为 20,799 / 7,959,327 / 69,562,625 / 10,273,459 / 8,415,769。

模型保存 16-bit token ID、8-bit 概率和 backoff、16-bit successor count，
context 分桶并带稀疏索引。概率编码 0–255；backoff 编码 0 精确表示零，
1–255 表示非零权重；header min 定点比例为 1e7，step 比例为 1e9。
读取器只接受 version 2；旧 version 1/Q16 明确拒绝。
词汇先验、Beam 参数及自学习逻辑保持不变。

虎娘由 `SentenceFivegram` 自己 mmap 文件并直接查询，不再编译或分发第三方
KenLM query runtime。`SentenceHistoryLanguageModel`、Beam history、锁定前缀、
回删、已上屏历史裁剪和 EOS 逻辑均保持原五阶实现不变。

## 行为

每条搜索路径保存最近四个 token ID 和历史长度；BOS 只进入一次，EOS 使用
完整 history。分数仍转换为自然对数。现有 Beam、码序、词汇先验、自学习、
选重和提前上屏策略不因模型后端替换而改变。

observed bigram 只有模型中实际存在该二元组时才返回 true。未知字符不会借用
`<unk>` 的二元组命中。冷启动模型缺失或损坏时保留普通码表输入；重载失败
不得替换已经有效的资源。

## 当前模型验证

使用独立 Python 参考读取器生成 2,103 条评分查询，冻结于
`tests/fivegram/q8-oracle.tsv`。上游报告属于探索性实验，不能当作虎娘实机准确率。

## 历史 Q8 量化迁移验证（2026-09-24）

便携和 Windows 测试均检查 Q8 冻结分数、1341 条独立 Python 参考查询、
增量输入、回删、锁定、取消、共享映射与资源生命周期。
`tests/fivegram_format_test.py --probe <fivegram_probe>` 构造小型 Q8 文件，
单独覆盖零 backoff、最大量化编码、截断、非法索引及旧版本拒绝。

2026-09-24 使用同一虎娘解码器、相同码表和参数重新运行 Q16/Q8 配对 20k：

| 模型 | 首选命中 | 首选变化 | 目标候选名次变化 |
| --- | ---: | ---: | ---: |
| 原 Q16（冻结旧读取器，仅离线对照） | 19930 / 20000 | — | — |
| 主线 Q8 | 19930 / 20000 | 0 | 0 |

该结果是迁移回归，不是独立泛化准确率；Q16 读取器和模型不随主线分发。
参考查询的允许误差为 1e-9；Linux 最大误差约 7.11e-15，Windows 各架构为 0。
详细记录见 `tests/fivegram/validation-20260924-q8.json`。

## 历史 Q16 后端迁移验证（2026-09-23）

当时的专项测试入口（当前源码对应 Q8）：

```sh
cmake -S tests/fivegram -B build/fivegram-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/fivegram-tests -j 4
build/fivegram-tests/fivegram_probe test data/Models/sentence-fivegram-mobile.bin build/fivegram-tests/fixture
```

Windows 对应 `tests/SentenceFivegramProbe.vcxproj`。测试覆盖冻结 TCSKNM03 分数、
OOV、observed bigram、增量、回删、锁定、取消、历史裁剪、资源生命周期和
损坏文件拒绝。Linux、Windows x64、Win32、ARM64、ARM64EC 的真实
460,693,519 字节模型均通过 1049 项专项检查；x64/Win32、ARM64、ARM64X
正式构建均通过。三架构资源导入/解码、fivegram 打包清单、核心回归、自动
提前上屏、自学习、decoder 与资源发布回归也均通过。

### 20k 首选迁移结果

同一虎娘 decoder、同一 Beam/先验，仅替换模型后端：

| 模式 | 旧集 / 10000 | 新集 / 10000 | 合计 / 20000 |
| --- | ---: | ---: | ---: |
| 迁移前 KenLM 五阶 + 五阶 observed bigram | 9958 | 9969 | 19927 |
| TCSKNM03 native + 五阶 observed bigram | 9959 | 9971 | 19930 |

共 4 条首选变化：3 条错误变正确，1 条错误变另一错误，0 条正确变错误。
变化为 `old_2357`、`fresh_5806`、`old_8167`、`fresh_208`。此前
`fresh_7690` 已在虎娘统一 observed bigram 时由错误变正确，因此这次迁移
不再产生变化。

该 20k 数据含训练文本重合，只用于迁移回归，不是独立泛化结论。

## 架构结果

正式生产链路现在是：

`SentenceDecoder -> SentenceHistoryLanguageModel -> SentenceFivegram(TCSKNM03) -> mmap`

仓库不再需要 `third_party/kenlm`、`Kenlm.props`、KenLM 特殊 C++14 编译规则
或 KenLM 许可证随包复制。三阶 `SentenceNgram` 仍保留为独立回归/旧格式工具，
不是生产五阶自动回退。
