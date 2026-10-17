当前皮肤命名

自制皮肤统一使用“角色-主题”；SSF 文件名与 skin.ini 的 skin_name 一致。
正式名称及旧文件名对应关系见 catalog.json 的 skins；归档项见 archived。
默认.ssf 是程序保留项。当前正式包共 12 套皮肤（含默认）。

虎娘-元气
虎娘-虎纹奶油
虎娘-蜜桃
虎娘-雨竹听风
虎娘-雪夜铃兰
虎娘-霓虹电玩
虎娘-桂月团圆
虎娘-柚光汽水
虎娘-幽焰小恶魔
虎兄贵-健身
鲸鱼娘-蓝鲸雪语

2026-10-18：虎娘-绯绒小恶魔、虎娘-莓糖小恶魔移入
皮肤/归档/20261018-小恶魔，源图和提示词保留原路径。默认构建不再生成；
正式打包同时排除归档名称及旧文件名，已有用户目录中的皮肤保留。
重建归档成品：python3 tools/build_little_devil_ssf.py --archived

构建脚本均可传入输出目录，例如 resources/Skins：
python3 tools/build_tigirl_trio_ssf.py resources/Skins
python3 tools/build_direction_ssf.py resources/Skins
python3 tools/build_little_devil_ssf.py resources/Skins
python3 tools/build_tiger_bro_ssf.py resources/Skins
python3 tools/build_blue_whale_ssf.py resources/Skins

项目皮肤目录为 皮肤；安装资源目录为 resources/Skins；本机用户目录为
%LOCALAPPDATA%\Tigirl\皮肤。当前改名已同步这三处，并更新本机当前皮肤引用。
改名不改变图片、布局或用户的其他设置。第三方皮肤、测试副本及历史归档保留原名。
catalog.json 的 archived 清单用于正式打包排除，不自动迁移或删除用户文件。

版本库保留方向原画、制作源素材、构建参数及成品 SSF。历史对照截图、
渲染报告、试验版素材与旧版压缩包仅保存在本机，不上传；历史说明中指向
这些 design 预览目录的路径是本地记录，不是发布附件。
