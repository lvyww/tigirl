# 虎纹奶油设置装饰

`tiger-cream-ornaments.png` 是内置 image_gen 根据选定的
`design/settings-640x480/tiger-decor/01-tiger-cream.png` 提取、补全的透明素材，
保留原始 1448×1086 RGBA 分辨率。生成参数与完整提示词见 `generation-prompt.txt`。

此文件是透明装饰图集，不是整窗截图；文字、原生控件、预览和按钮均实时绘制。
`tools/SettingsSkin.cpp` 将图集坐标归一化为 640×480，按源区域分别取外框、
蝴蝶结铃铛和小虎娘；小虎娘只绘制在候选外观页。PNG 由 `SchemaManager.rc`
以资源 14 嵌入，运行时无需读取磁盘图片。原赞赏码仍使用独立资源 13。

WIC 将 PNG 解码为预乘 BGRA 位图并缓存；Direct2D 在切页和重绘时复用位图，
设备目标丢失时从缓存重建。高对比度模式不绘制此图集。

页面不再绘制单独底板；窗口统一使用奶油背景。尾巴从图集中完整取出并缩放到固定底栏，避免页面边缘截断尾巴。

`tiger-cream-detached-ornaments.png` 是从原图通过 imagegen 单独提取的蝴蝶结铃铛、虎尾与爪印（1881×836 RGBA 原图，资源 15）。绘制时使用独立源矩形，避免把原边框随装饰缩放、移动。窗口左下角使用右下角的干净边框镜像补全；爪印放在底栏空白处，避开页面与取消按钮。生成提示词保留于 `detached-generation-prompt.txt`。
