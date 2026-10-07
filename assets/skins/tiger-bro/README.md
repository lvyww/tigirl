# 虎兄贵

根据用户提供的虎兄贵头像设计：壮硕成年拟人虎、黑色背心、抱臂姿态，配炭黑及琥珀色边框、浅奶油色文字区。
使用内置 image_gen 生成透明素材，提示词见 generation.txt；panel.png 为初版素材，panel-gym.png 为当前放大角色、增加左上角哑铃的版本；编辑提示词见 gym-generation.txt。

执行 `python3 tools/build_tiger_bro_ssf.py` 生成项目 `皮肤/虎兄贵.ssf`。
适用于当前虎娘五种显示模式，横排无编码及仅编码为单行，其他模式按内容增高。
基准字号 200；用户设置字体和字号仍生效，字号等比缩放整个窗口。
这是静态插画皮肤。角色位于顶部固定区域，边框中段拉伸以容纳内容。

已经通过原生渲染器 5 种布局 × 3 种内容状态 × 4 档 DPI 的 60 个用例，
包括候选点击区域、空码替代、透明像素、字号和 DPI 等比缩放检查。
preview-horizontal.png 是 200% DPI 的实际渲染示例，灰底为测试中的第二项悬停状态。

安装位置：`%LOCALAPPDATA%\Tigirl\皮肤\虎兄贵.ssf`。刷新皮肤列表后选择“虎兄贵”。

2026-10-07：当前打包脚本改用 *-narrow.png / panel-narrow.png，画布宽 1536、基准字号 200。字号 17、100% DPI 时最小宽度 131 像素。本机用户目录、项目皮肤目录及 resources/Skins 已同步；窄版提示词见 assets/skins/default-width-prompts.json。
