2026-10-18 归档：绯绒小恶魔、莓糖小恶魔

此目录不属于正式打包资源目录。两个 SSF 保留归档前原始字节。
源图、生成提示词及历史预览仍保留原路径：
  assets/skins/little-devil-20261017/crimson-velvet
  assets/skins/little-devil-20261017/strawberry-candy
归档参数：assets/skins/little-devil-20261017/archived-skins.json。
在仓库根目录重建归档：
  python3 tools/build_little_devil_ssf.py --archived
正式默认构建仅生成幽焰小恶魔；归档文件即使误放回 resources/Skins，
打包来源筛选也会按 assets/skins/catalog.json 的 archived 清单排除。
已安装用户目录中的已有皮肤不作删除。
