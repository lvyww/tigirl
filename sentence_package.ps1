$NativeTigerSentenceModelHash = '6485240EB1A6ACBA3AA54EF4FDBB1F34C5D4B70A1CAAFF20AA2F296D5ACA8B52'
function Assert-NativeTigerSentenceModel([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing sentence model: $Path" }
    if ((Get-Item -LiteralPath $Path).Length -ne 238789568 -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $NativeTigerSentenceModelHash) {
        throw 'Sentence model does not match the validated non-neural reference model.'
    }
}
function Copy-NativeTigerSentenceModel([string]$Source, [string]$Destination) {
    Assert-NativeTigerSentenceModel $Source
    $folder = Join-Path $Destination 'Models'
    New-Item -ItemType Directory -Force $folder | Out-Null
    $target = Join-Path $folder 'sentence-ngram-v2.bin'
    if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target).Hash -ne $NativeTigerSentenceModelHash) {
        Copy-Item -LiteralPath $Source -Destination $target -Force
    }
    Assert-NativeTigerSentenceModel $target
    [ordered]@{ model='sentence-ngram-v2.bin'; bytes=238789568; sha256=$NativeTigerSentenceModelHash.ToLowerInvariant();
        source='User-supplied TigerClaw release_arm64/Models/sentence-ngram-v2.bin';
        scope='Non-neural unigram/bigram/trigram model; no Qwen runtime' } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $folder 'provenance.json') -Encoding UTF8
}
