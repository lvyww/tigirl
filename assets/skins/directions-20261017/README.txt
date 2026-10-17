2026-10-17 四款候选窗皮肤

本批仅实现方向稿 02 雨竹听风、03 雪夜铃兰、07 霓虹电玩、09 深海舰长。
前 3 款为虎娘，09 为虎兄贵。每款含独立横排、竖排底图。

制作
  使用内置 image_gen 编辑 design/skin-directions-20261017 中的视觉方向稿。
  generation.json 保存初始提示词；*-prompts.json、*-prompt.txt 和
  *-refinement.txt 保存透明边缘及拉伸区迭代。generated-sources.json 记录
  原始生成文件。skins.json 指定实际采用的 8 张 PNG、文字颜色、边距和切片。
  其余 PNG 是制作草稿，不进入 SSF。源图均原样保留透明通道。

构建
  python3 tools/build_direction_ssf.py build/skin-v1 --assets assets/skins/directions-20261017
  本目录保留第一版。脚本默认使用首版 PNG 与三款 font_size=100 调参布局；
  重建原始第一版请显式指定上述
  --assets 和独立输出目录。只使用 Python 标准库；同一来源结果逐字节一致。
  原稿放大 2 倍试用版：
  python3 tools/build_direction_ssf.py "皮肤/旧稿放大2倍" --assets assets/skins/directions-20261017 --art-scale 2
  这只调整 SSF 坐标基准与文字间距，不更改八张原始 PNG；文字大小仍
  取用户设置，角色、装饰、边框与画布留白按原图一起放大。
  每个 SSF 包含 skin.ini、horizontal.png、vertical.png，全部低于运行时的
  4 MiB 限制。原图不裁切、不改色、不重新编码。

排版
  SSF 字号 200 是源图坐标基准；实际字号、字体仍由用户设置决定。
  横竖六种 Scheme 均配置，支持编码、候选、合并及隐藏候选的显示模式。
  中间的 1 像素切片供九宫格延展，角色留在固定角落；候选文字实时绘制。
  这些是静态皮肤，不含逐帧动画；现有候选窗出入动效保持不变。

验证
  用当前源码的 ARM64 SsfProbe、SsfRenderProbe 在 Windows 上验证。
  四款共 320 个渲染组合：5 种布局 × 4 种内容 × 4 档 DPI。
  内容覆盖 3 候选、5 候选、无候选、长句；DPI 为 96/120/144/192。
  同时检查命中区、透明像素、字体生效和字号/DPI 等比缩放；60 个接缝回归通过。
  design/skin-pack-20261017/index.html 提供实际渲染图和背景切换。
  design/skin-pack-20261017/verification.json 记录文件哈希与结果。

分发
  成品位于根目录“皮肤”，并同步到 resources/Skins，供下次安装包构建收集。
  皮肤包不替换输入法 DLL、码表、用户配置，不自动更改当前主题。
  还没有为这批皮肤重新生成安装程序；本次交付独立 SSF 压缩包。
