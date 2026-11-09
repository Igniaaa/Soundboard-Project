param()
$ErrorActionPreference = 'Stop'
$taskCommit = 'd53d998e2bc3fb3ca394f1ee5b0c2f3886a67331'
$taskHash = '2B7797725272193E54C9BEC660E30918A6171C00B510979FA7CE86E21AAA3DDF'
$taskArchive = Join-Path $PSScriptRoot 'deps\foleys_gui_magic-d53d998.zip'
$taskDependency = Join-Path $PSScriptRoot ('deps\foleys_gui_magic-' + $taskCommit)
New-Item -ItemType Directory -Path (Split-Path $taskArchive -Parent) -Force | Out-Null
if (-not (Test-Path -LiteralPath $taskArchive)) {
    Invoke-WebRequest -Uri ('https://codeload.github.com/ffAudio/foleys_gui_magic/zip/' + $taskCommit) -OutFile $taskArchive
}
if ((Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash -ne $taskHash) {
    throw 'Archivio Foleys GUI Magic non corrispondente alla versione prevista.'
}
if (-not (Test-Path -LiteralPath $taskDependency)) {
    Expand-Archive -LiteralPath $taskArchive -DestinationPath (Join-Path $PSScriptRoot 'deps')
}
if (-not (Test-Path -LiteralPath (Join-Path $taskDependency 'modules\foleys_gui_magic\foleys_gui_magic.h'))) {
    throw 'Modulo Foleys GUI Magic non trovato nella cartella prevista.'
}
Write-Output ('Foleys GUI Magic pronto: ' + $taskCommit)
