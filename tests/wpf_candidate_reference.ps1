$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationFramework
$fontRoot = [IO.Path]::GetFullPath("$PSScriptRoot\..\data\fonts")
$output = [IO.Path]::GetFullPath("$PSScriptRoot\..\build\wpf-render-reference")
New-Item -ItemType Directory -Force $output | Out-Null
$family = [Windows.Media.FontFamily]::new([uri]($fontRoot+'\'),'./#霞鹜文楷 GB 屏幕阅读版')
foreach ($dpi in @(96,120,144,192)) {
    $panel = [Windows.Controls.StackPanel]::new()
    $panel.Margin = [Windows.Thickness]::new(6)
    foreach ($line in @('ab 〔1/2〕','1 交 [ab]','2 疒 · 注释','3 测试汉字 Aa 123','4 𠮷 é 😀')) {
        $text = [Windows.Controls.TextBlock]::new()
        $text.Text = $line
        $text.FontFamily = $family
        $text.FontSize = 17
        $text.Height = 25.5
        $text.VerticalAlignment = [Windows.VerticalAlignment]::Center
        [void]$panel.Children.Add($text)
    }
    $border = [Windows.Controls.Border]::new()
    $border.Child = $panel
    $border.Background = [Windows.Media.BrushConverter]::new().ConvertFromString('#FFFFF8F3')
    $border.BorderBrush = [Windows.Media.BrushConverter]::new().ConvertFromString('#FF1A7B6B')
    $border.BorderThickness = [Windows.Thickness]::new(1.25)
    $border.CornerRadius = [Windows.CornerRadius]::new(5)
    $border.UseLayoutRounding = $true
    $border.SnapsToDevicePixels = $true
    $border.Measure([Windows.Size]::new(1000,1000))
    $border.Arrange([Windows.Rect]::new([Windows.Point]::new(0,0),$border.DesiredSize))
    $border.UpdateLayout()
    $bitmap = [Windows.Media.Imaging.RenderTargetBitmap]::new([int][Math]::Ceiling($border.ActualWidth*$dpi/96),[int][Math]::Ceiling($border.ActualHeight*$dpi/96),$dpi,$dpi,[Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($border)
    $encoder = [Windows.Media.Imaging.PngBitmapEncoder]::new()
    $encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $stream = [IO.File]::Create((Join-Path $output "wpf-$dpi.png"))
    try { $encoder.Save($stream) } finally { $stream.Dispose() }
}
@{status='rendered';dpi=@(96,120,144,192);font_family=$family.Source;purpose='Visual reference; not pixel-equivalent layout or live WPF window acceptance'} | ConvertTo-Json | Set-Content (Join-Path $output 'report.json') -Encoding UTF8
