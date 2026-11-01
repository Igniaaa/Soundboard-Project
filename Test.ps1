param(
    [switch]$AudioDevices, [switch]$Performance, [string]$ToolRoot = $PSScriptRoot,
    [string]$Executable = '', [string]$BuildDirectory = '', [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$taskBuild = if ($BuildDirectory) { [System.IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $PSScriptRoot 'build-msvc' }
$taskExecutable = if ($Executable) { [System.IO.Path]::GetFullPath($Executable) } else { Join-Path $taskBuild 'bin\DualDeckJuce.exe' }
$taskOutput = if ($OutputDirectory) { [System.IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $PSScriptRoot 'test-output' }
if (-not (Test-Path -LiteralPath $taskExecutable)) { throw 'Eseguibile di test assente. Esegui prima Build.ps1.' }
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
& (Join-Path $ToolRoot 'tools\cmake-4.4.4-windows-x86_64\bin\ctest.exe') --test-dir $taskBuild -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Test DSP non superati.' }
function Invoke-DeckCheck([string]$taskArguments) {
    $taskProcess = Start-Process -FilePath $taskExecutable -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
    if (-not $taskProcess.WaitForExit(60000)) { Stop-Process -Id $taskProcess.Id; throw 'Test scaduto dopo 60 secondi.' }
    if ($taskProcess.ExitCode -ne 0) { throw ('Test fallito: ' + $taskArguments) }
}
Invoke-DeckCheck ('--self-test "' + $taskOutput + '"')
Invoke-DeckCheck ('--keyboard-smoke "' + (Join-Path $taskOutput 'keyboard-devices.txt') + '"')
Invoke-DeckCheck ('--interaction-smoke "' + (Join-Path $taskOutput 'ui-interactions.txt') + '"')
foreach ($taskSection in @(0,1,2,3,4)) {
    $taskName = @('board.png','audio.png','keyboard.png','dedicated-keyboard.png','editor.png')[$taskSection]
    Invoke-DeckCheck ('--smoke-test "' + (Join-Path $taskOutput $taskName) + '" ' + $taskSection)
}
Invoke-DeckCheck ('--smoke-test "' + (Join-Path $taskOutput 'board-small.png') + '" 0 960 640')
Invoke-DeckCheck ('--smoke-test "' + (Join-Path $taskOutput 'editor-small.png') + '" 4 960 640')
Invoke-DeckCheck ('--smoke-test "' + (Join-Path $taskOutput 'editor-effects.png') + '" 5')
Invoke-DeckCheck ('--smoke-test "' + (Join-Path $taskOutput 'editor-effects-small.png') + '" 5 960 640')
if ($AudioDevices) { Invoke-DeckCheck ('--audio-smoke "' + (Join-Path $taskOutput 'audio-devices.txt') + '"') }
if ($Performance) { Invoke-DeckCheck ('--performance-test "' + (Join-Path $taskOutput 'performance.txt') + '"') }
Get-Content -LiteralPath (Join-Path $taskOutput 'library-tests.txt')
Write-Output ('Anteprime e report: ' + $taskOutput)
