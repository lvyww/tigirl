# 蓝鲸雪语

panel.png 保留用户提供的原图。panel-large.png 为上一次放大版。当前使用 panel-narrow.png：进一步收窄窗口并按角色及鲸鱼再增大约 30% 的目标编辑；内置 image_gen 提示词见 narrow-prompt.txt。字号 17、100% DPI 下最小宽度为 131 像素。
执行 `python3 tools/build_blue_whale_ssf.py` 生成 `皮肤/蓝鲸雪语.ssf`。

支持虎娘的五种显示模式；设置字体仍生效，字号等比缩放图片、文字和边距。
原图坐标中的固定边角避开角色、鲸鱼与蝴蝶结，文字使用深蓝色，首选候选使用亮蓝色。
静态皮肤，无逐帧动画。字号基准不变，文字内边距按新图重新定位。

已通过五种布局、三种内容状态、四档 DPI 共 60 个新皮肤渲染用例，另有默认皮肤回归用例。
preview-horizontal.png 为实际渲染示例；第二项灰底是测试悬停状态。
安装到 `%LOCALAPPDATA%\Tigirl\皮肤\蓝鲸雪语.ssf`；刷新皮肤列表后选择。
