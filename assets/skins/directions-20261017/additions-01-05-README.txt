虎娘 · 桂月团圆 / 柚光汽水（2026-10-17）

根据 design/skin-directions-20261017 中用户选定的 01 / 05 方向稿制作。
采用内置 image_gen 生成与编辑；未使用 API/CLI 后备方式。

最终使用素材（RGBA，保持生成文件的原始 PNG 字节）：
  moon-osmanthus/horizontal-final.png
  moon-osmanthus/vertical-final.png
  grapefruit-soda/horizontal-final.png
  grapefruit-soda/vertical-production.png
横排为 1774 × 887，竖排为 1254 × 1254。
其它同目录 PNG 是制作阶段的迭代存档，不打入 SSF。
其中 moon-osmanthus/vertical-production.png 存在透明缺口，已被最终版修复。

生产参数：../directions-20261017-tuned/skins.json。
font_size=100；candidate_spacing=60；line_spacing=12；code_gap=16。
候选字、拼音、编码和选中状态均由输入法绘制，不烘焙到图片。
兼容横排带编码、竖排带编码、横排无编码、竖排无编码和仅编码。
字符数量增加时，仅扩展中部边框和底板；人物、果片、桂花、月亮保留原比例。
需要包含 min_width / min_height 支持和高质量缩小的新程序才能获得全部效果。

提示词与生成记录：
  additions-01-05-prompts.json（方向稿转换为四张独立素材）
  additions-01-05-refinement-prompts.json（伸缩区域调整）
  grapefruit-soda/horizontal-cleanup-prompt.txt（横排边饰避开伸缩带）
  moon-osmanthus/final-prompts.json（纸面透明缺口修复、横排边饰调整）
  additions-01-05-generated.json（生成路径、最终选择、文件哈希）

重建：
  python3 tools/build_direction_ssf.py --skin 01 --skin 05
  python3 tools/build_direction_ssf.py resources/Skins --skin 01 --skin 05
新素材加入默认构建，原三款的 SSF 字节保持不变。
交付到“皮肤”、resources/Skins 和当前用户的 Tigirl/皮肤目录；不改当前主题选择。
此前已完成的安装包不包含本次新增资源，下次完整打包会自动收集。

实际渲染预览和完整验证记录：
  ../../../design/skins-01-05-20261017/index.html
  ../../../design/skins-01-05-20261017/validation.json
