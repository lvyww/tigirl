虎娘 · 小恶魔主题三款（2026-10-17）

01 绯绒小恶魔（2026-10-18 归档）：红黑丝绒、香槟金、蝴蝶结与铃铛。
02 莓糖小恶魔（2026-10-18 归档）：莓粉、奶油白、淡紫，软糖心形与糖果叉。
03 幽焰小恶魔：紫黑底板、青色魔焰、银紫边框，魔法书与蝙蝠翼。

方向稿：../../../design/little-devil-directions-20261017/index.html
采用内置 image_gen 提取和编辑透明素材，未使用 API/CLI 后备方式。
素材原始 PNG 字节保留；不通过脚本重绘、裁剪或缩放角色。
候选文字、编码、注释和选中状态由输入法实时绘制，图片不烘焙文字。

生产定义：skins.json，仅包含 03 幽焰小恶魔。
归档定义：archived-skins.json，包含 01 / 02；成品位于
  ../../../皮肤/归档/20261018-小恶魔
最终 PNG 以对应定义中各方向的 file 字段为准；其余 PNG 为制作迭代存档。
高清源图、输入参考、完整提示词及生成文件哈希保存在本目录：
  prompts.json
  refinement-prompts.json
  crimson-velvet/ribbon-cleanup-prompt.json
  generated.json

font_size=140；candidate_spacing=84；line_spacing=17；code_gap=22。
边距参照虎娘-虎纹奶油，结合每张原图的文字区与装饰安全区域校准。
说明见 ../font140-calibration-20261017.txt；当前预览见
  ../../../design/skins-font140-20261017/index.html
实际字号、字体、排列方式和候选数量仍使用用户设置。
支持横排带编码、竖排带编码、横排无编码、竖排无编码和仅编码五种布局。
九宫格伸缩区避开角色和主要饰物；min_width / min_height 压缩空白，保护固定端点。
源图内部底板保持不透明，外部透明。旧程序忽略最小尺寸扩展字段；获得紧凑布局
和高质量图片缩小效果需要包含这些支持的新程序。

重建（仓库根目录，Python 标准库）：
  python3 tools/build_little_devil_ssf.py
  python3 tools/build_little_devil_ssf.py resources/Skins
默认只构建正式款 03。重建归档到归档目录：
  python3 tools/build_little_devil_ssf.py --archived
只重建归档款 02：
  python3 tools/build_little_devil_ssf.py --archived --skin 02
构建脚本只生成选中的 SSF，不清理目标目录内其它皮肤。
复用 tools/build_direction_ssf.py 的封装器，SSF 内为 skin.ini、horizontal.png、vertical.png。
每包须小于运行时 4 MiB 限制。

交付目录：项目“皮肤”、resources/Skins、当前用户 %LOCALAPPDATA%/Tigirl/皮肤。
当前选中的主题与其它用户配置保持不变。在“候选外观”点击“刷新”后选择新主题。
安装器收集 resources/Skins，并按 catalog.json 排除归档款及旧文件名。
下次完整打包仅包含正式款；已有安装包不会自动更新，用户已有归档款不作删除。

首次 font_size=100 交付的历史预览与验证记录：
  ../../../design/little-devil-skins-20261017/index.html
  ../../../design/little-devil-skins-20261017/validation.json
