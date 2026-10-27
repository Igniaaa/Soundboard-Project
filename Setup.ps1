param([switch]$AcceptToolchainLicense)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskDownloads = @(
    @{ Name='JUCE 9.0.4'; Url='https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.4.zip'; File='deps\JUCE-9.0.4.zip'; Destination='deps'; Check='deps\JUCE-9.0.4\CMakeLists.txt'; Hash='16AD4CFCA70B73DF6352E4E840BA5DC2E755D36B8AB2DD976EF3ECEDCD18F6A9' },
    @{ Name='CMake 4.4.4'; Url='https://github.com/Kitware/CMake/releases/download/v4.4.4/cmake-4.4.4-windows-x86_64.zip'; File='tools\cmake.zip'; Destination='tools'; Check='tools\cmake-4.4.4-windows-x86_64\bin\cmake.exe'; Hash='BACE36E94B31C68AB6FA295F26DFA11219E0701CF7C94B0284A7D1CB13DAC536' },
    @{ Name='Ninja 1.13.2'; Url='https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip'; File='tools\ninja.zip'; Destination='tools\ninja'; Check='tools\ninja\ninja.exe'; Hash='07FC8261B42B20E71D1720B39068C2E14FFCEE6396B76FB7A795FB460B78DC65' }
)
foreach ($taskItem in $taskDownloads) {
    if (Test-Path -LiteralPath (Join-Path $taskRoot $taskItem.Check)) { continue }
    $taskFile = Join-Path $taskRoot $taskItem.File
    New-Item -ItemType Directory -Path (Split-Path $taskFile -Parent) -Force | Out-Null
    if (-not (Test-Path -LiteralPath $taskFile)) { Write-Output ('Download: ' + $taskItem.Name); Invoke-WebRequest -Uri $taskItem.Url -OutFile $taskFile }
    if ((Get-FileHash -LiteralPath $taskFile -Algorithm SHA256).Hash -ne $taskItem.Hash) { throw ('Hash non valido: ' + $taskItem.Name) }
    Expand-Archive -LiteralPath $taskFile -DestinationPath (Join-Path $taskRoot $taskItem.Destination) -Force
}
if (-not (Test-Path -LiteralPath (Join-Path $taskRoot 'tools\msvc\setup_x64.bat'))) {
    Write-Output 'Preparazione Microsoft C++ e Windows SDK nella cartella tools. Lo script mostra i termini Microsoft se non hai passato -AcceptToolchainLicense.'
    $taskArguments = @((Join-Path $taskRoot 'scripts\portable-msvc.py'),'--vs','2022','--msvc-version','14.44','--sdk-version','26100','--host','x64','--target','x64')
    if ($AcceptToolchainLicense) { $taskArguments += '--accept-license' }
    Push-Location (Join-Path $taskRoot 'tools')
    try { & python @taskArguments; if ($LASTEXITCODE -ne 0) { throw 'Preparazione compilatore non riuscita.' } }
    finally { Pop-Location }
}
Write-Output 'Dipendenze pronte. Esegui Build.ps1.'
