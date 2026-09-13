# Tigirl 品牌图标

`Tigirl.png` 为用户提供的透明原图。`Tigirl.ico` 保留透明背景并留出小幅边距，包含 16、20、24、32、40、48、64、96、128、256 像素图层。

重新转换：`python tools/make_icon.py assets/Tigirl.png assets/Tigirl.ico`（需要 Pillow）。

该图标用于输入法 Profile、DLL、桌面工具和安装对话框；中／英模式状态图标继续使用可辨认的文字。

`Tigirl-donate.png` 为用户提供的赞赏码原图，复制自参考工程的 `赞赏码.png`，字节未改动。用于设置窗口“赞赏”标签页，内嵌资源编号 13。

`Tigirl-wizard-source.png` 为用户提供的安装向导大图原稿。`Tigirl-wizard.bmp`
将其等比例缩放、居中放在 164×314 的暖白色背景上；安装器右上角小图仍使用原品牌图标。
同时重新生成图标和安装向导位图：

```sh
python tools/make_icon.py assets/Tigirl.png assets/Tigirl.ico --wizard --wizard-large-source assets/Tigirl-wizard-source.png
```
