$NativeTigerSentenceModelHash = '580ED90CED0AC72E453E0D647635CEC2D3879E2E47AE1A231B96F49B2D34EAFA'
$NativeTigerSentenceLexicalHash = '8DBC884B6CB719D07E4CEF153C8048DB19A11F8224F75A4ED87853E688A27393'
$NativeTigerSentenceLexicalPath = Join-Path $PSScriptRoot 'resources\sentence-lexical-v1.bin'
function Assert-NativeTigerSentenceModel([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing sentence model: $Path" }
    if ((Get-Item -LiteralPath $Path).Length -ne 419929926 -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $NativeTigerSentenceModelHash) {
        throw 'Sentence model does not match the validated non-neural reference model.'
    }
}
function Assert-NativeTigerSentenceLexicalPrior([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-Item -LiteralPath $Path).Length -ne 150032 -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $NativeTigerSentenceLexicalHash) {
        throw 'Sentence lexical prior does not match the validated TCSLEX01 artifact.'
    }
}
function Copy-NativeTigerSentenceModel([string]$Source, [string]$Destination) {
    Assert-NativeTigerSentenceModel $Source
    $folder = Join-Path $Destination 'Models'
    New-Item -ItemType Directory -Force $folder | Out-Null
    $target = Join-Path $folder 'sentence-fivegram.klm'
    if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target).Hash -ne $NativeTigerSentenceModelHash) {
        Copy-Item -LiteralPath $Source -Destination $target -Force
    }
    Assert-NativeTigerSentenceModel $target
    $licenses = Join-Path $Destination 'licenses\kenlm'
    New-Item -ItemType Directory -Force $licenses | Out-Null
    foreach ($name in @('LICENSE','COPYING','COPYING.3','COPYING.LESSER.3','UPSTREAM.json','TIGERCLAW_PATCHES.md','TIGIRL_INTEGRATION.md')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "third_party\kenlm\$name") -Destination $licenses -Force
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\kenlm\util\double-conversion\LICENSE') -Destination (Join-Path $licenses 'double-conversion-LICENSE') -Force
    Assert-NativeTigerSentenceLexicalPrior $NativeTigerSentenceLexicalPath
    $lexicalTarget = Join-Path $folder 'sentence-lexical-v1.bin'
    if (!(Test-Path -LiteralPath $lexicalTarget) -or
        (Get-FileHash -LiteralPath $lexicalTarget -Algorithm SHA256).Hash -ne $NativeTigerSentenceLexicalHash) {
        Copy-Item -LiteralPath $NativeTigerSentenceLexicalPath -Destination $lexicalTarget -Force
    }
    Assert-NativeTigerSentenceLexicalPrior $lexicalTarget
    [ordered]@{ model='sentence-fivegram.klm'; bytes=419929926; sha256=$NativeTigerSentenceModelHash.ToLowerInvariant();
        lexical_prior='sentence-lexical-v1.bin'; lexical_bytes=150032; lexical_sha256=$NativeTigerSentenceLexicalHash.ToLowerInvariant();
        source='experiments/brightmart-char5-500mb-20260922/char5-context128-q8.klm; TigerClaw e74a3a6, pure-character fivegram';
        source_sha256='580ed90ced0ac72e453e0d647635cec2d3879e2e47ae1a231b96f49b2d34eafa';
        scope='Fivegram search and observed-bigram prior; no trigram dependency or Qwen runtime' } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $folder 'provenance.json') -Encoding UTF8
}
