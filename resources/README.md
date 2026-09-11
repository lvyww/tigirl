# 默认方案数据

仓库包含两个独立方案，以及它们共用的拼音反查表：

```text
DefaultData/
  码表/
    虎整句/       # 整句源码表、快符、补充语料
    虎码字词/     # 字表、词表、简词、注释、拆分、符号和快符
  拼音反查码表/
    拼音.txt
```

数据取自已制作的 `Tigirl-2026.9.10.4-x64` 发布包的 `DefaultData`，
保留原文件内容。它是固定发布快照，不跟随本机用户目录或虎爪目录的修改。
`default-data-manifest.json` 记录来源、文件长度和 SHA256；更新数据时应同步更新清单。
`.gitattributes` 禁用这些数据文件的自动换行转换，确保 Windows 与 Linux 校验一致。

这里只存放源数据，不包含用户配置、词频日志、编译缓存、字体或整句模型。
导入“虎整句”源码表不需要统计模型；完整整句解码及效果评估仍需另行提供模型。

## 测试

Linux 无 Windows 环境时，可先检查源码表文件完整性：

```sh
python tests/bundled_schemas_test.py --verify-only
```

Windows 安装 Visual Studio C++ 工具及 Windows SDK 后：

```powershell
./build_native.ps1 -Platform x64,Win32
python tests/bundled_schemas_test.py --platform x64
python tests/bundled_schemas_test.py --platform Win32
```

WSL 可在 Windows 工具已编译后运行同样的 Python 测试。
测试在临时目录中分别导入两个方案，使用生产导入器校验生成的二进制码表，
检查 `ab → 交`、`ni → 你`、字词方案的注释/拆分，以及整句词库和补充语料缓存。
测试不会注册或安装输入法，不需要模型；Windows CI 执行这两种架构的导入检查。
Linux CI 仅执行源数据完整性检查及现有合成核心测试。

## 手动编译码表

先编译对应架构的工具，然后在 PowerShell 中执行：

```powershell
$source = (Resolve-Path ./resources/DefaultData).Path
$output = Join-Path $PWD 'build/bundled-dictionaries'
New-Item -ItemType Directory -Force $output | Out-Null
foreach ($scheme in @('虎整句', '虎码字词')) {
    & ./build/tests/x64/Tigirl.Import.exe "$source/码表/$scheme" `
        "$source/拼音反查码表" "$output/$scheme.tcd" 'zh-CN'
    if ($LASTEXITCODE) { throw "导入失败：$scheme" }
}
```

生成普通 `.tcd` 及 `.sentence.tcd`、`.supplement.tcd` 两个伴随文件。
输出应使用尚不存在的文件路径，导入器不会覆盖已有不可变码表。

`package_x64.ps1` 默认从此目录取得方案数据；`-DataSource` 仍可指定其他源目录。
模型路径现在由独立的 `-SentenceModelPath` 参数提供。完整发布构建仍需要字体、
预备运行资源和经过校验的模型，本次没有放宽发布校验或将编译产物提交到 Git。
