# 虎娘 TSF 标识

虎娘使用固定的产品专属 GUID，不再沿用微软 SampleIME 的自定义标识。

- CLSID：`{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}`
- 中文 Profile：`{43201C7B-F615-469D-9D54-906D9270975E}`
- TIP：`0804:{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}{43201C7B-F615-469D-9D54-906D9270975E}`

`SampleIME/Globals.cpp` 定义全部 13 个产品 GUID，包括保留键、自定义 compartment、语言栏、显示属性和候选 UI 标识。兼容系统所要求的 GUID（例如 `GUID_LBI_INPUTMODE`、TSF 类别与系统 compartment）保持不变。这些产品 GUID 必须跨版本、跨架构保持稳定。

`packaging/legacy_identity.ps1` 只用于清理历史微软示例 ID。新注册验证成功后，才考虑清理旧注册；GUI 安装在提交事务之后执行。必须确认所有旧 COM 注册视图都指向 Program Files/Tigirl/versions 内的 DLL，且与安装清单或本机开发安装记录中的 SHA-256 一致。遇到用户级覆盖、第三方路径、丢失或被修改的文件，一律保留旧注册。保留旧注册时也保留其引用的旧程序目录。不要直接按旧 GUID 删除注册表，也不要对来源不明的 DLL 调用反注册。

跨标识升级不支持自动回滚到旧标识 DLL；旧标识安装在新安装提交前保留。码表、用户词、调序、自学习和设置路径不变。已运行的应用可能仍加载旧 DLL，升级后应重启应用；系统输入法列表异常时注销后重新登录。

验证：`tests/legacy_identity_test.ps1` 检查真实清单哈希和跨架构归属拒绝规则；`candidate_layout_test.py --platform x64/Win32` 验证新 CLSID 的 TSF 激活及候选行为。
