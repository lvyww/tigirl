$NativeTigerSentenceModelHash = '756F6C92CF43AD6E8E3087CE66B711AC6AD0FC41E6F3FB82B3766E35ECAB8681'
$NativeTigerSentenceLexicalHash = '8DBC884B6CB719D07E4CEF153C8048DB19A11F8224F75A4ED87853E688A27393'
$NativeTigerSentenceLexicalPath = Join-Path $PSScriptRoot 'resources\sentence-lexical-v1.bin'
function Assert-NativeTigerSentenceModel([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing sentence model: $Path" }
    if ((Get-Item -LiteralPath $Path).Length -ne 405663171 -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $NativeTigerSentenceModelHash) {
        throw 'Sentence model does not match the validated TCSKNM03 v2 Q8 fivegram model.'
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
    $target = Join-Path $folder 'sentence-fivegram-mobile.bin'
    if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target).Hash -ne $NativeTigerSentenceModelHash) {
        Copy-Item -LiteralPath $Source -Destination $target -Force
    }
    Assert-NativeTigerSentenceModel $target
    Assert-NativeTigerSentenceLexicalPrior $NativeTigerSentenceLexicalPath
    $lexicalTarget = Join-Path $folder 'sentence-lexical-v1.bin'
    if (!(Test-Path -LiteralPath $lexicalTarget) -or
        (Get-FileHash -LiteralPath $lexicalTarget -Algorithm SHA256).Hash -ne $NativeTigerSentenceLexicalHash) {
        Copy-Item -LiteralPath $NativeTigerSentenceLexicalPath -Destination $lexicalTarget -Force
    }
    Assert-NativeTigerSentenceLexicalPrior $lexicalTarget
    [ordered]@{ model='sentence-fivegram-mobile.bin'; format='TCSKNM03'; format_version=2; quantization_bits=8; bytes=405663171; sha256=$NativeTigerSentenceModelHash.ToLowerInvariant();
        lexical_prior='sentence-lexical-v1.bin'; lexical_bytes=150032; lexical_sha256=$NativeTigerSentenceLexicalHash.ToLowerInvariant();
        source='Corpus4 50% + Articles 25% + Brightmart non-news 25%; pruned TCSKNM03 v2 Q8; corpus4-articles-third-20260925';
        source_sha256='756f6c92cf43ad6e8e3087ce66b711ac6ad0fc41e6f3fb82b3766e35ecab8681';
        scope='Fivegram search and observed-bigram prior; no trigram, KenLM, or Qwen runtime dependency' } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $folder 'provenance.json') -Encoding UTF8
}
