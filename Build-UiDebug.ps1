param([string]$ToolRoot = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskCmake = Join-Path $ToolRoot 'tools\cmake-4.4.4-windows-x86_64\bin\cmake.exe'
$taskMsvcRoot = Join-Path $ToolRoot 'tools\msvc'
if (-not (Test-Path -LiteralPath $taskCmake)) { throw 'Esegui prima Setup.ps1 per installare gli strumenti portatili.' }
$taskMsvc = Get-ChildItem -LiteralPath (Join-Path $taskMsvcRoot 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$taskSdkRoot = Join-Path $taskMsvcRoot 'Windows Kits\10'
$taskSdk = Get-ChildItem -LiteralPath (Join-Path $taskSdkRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $taskMsvc -or -not $taskSdk) { throw 'Compilatore Microsoft o Windows SDK mancante. Esegui Setup.ps1.' }
$taskOld = @{ PATH=$env:PATH; INCLUDE=$env:INCLUDE; LIB=$env:LIB }
$taskBuild = Join-Path $taskRoot 'build-ui-debug'
try {
    $env:PATH = (Join-Path $taskMsvc.FullName 'bin\Hostx64\x64') + ';' + (Join-Path $taskSdkRoot ('bin\' + $taskSdk.Name + '\x64')) + ';' + (Join-Path $ToolRoot 'tools\ninja') + ';' + $env:PATH
    $taskIncludes = @((Join-Path $taskMsvc.FullName 'include'))
    foreach ($taskPart in @('ucrt','shared','um','winrt','cppwinrt')) { $taskIncludes += Join-Path $taskSdk.FullName $taskPart }
    $env:INCLUDE = $taskIncludes -join ';'
    $env:LIB = @((Join-Path $taskMsvc.FullName 'lib\x64'),(Join-Path $taskSdkRoot ('Lib\' + $taskSdk.Name + '\ucrt\x64')),(Join-Path $taskSdkRoot ('Lib\' + $taskSdk.Name + '\um\x64'))) -join ';'
    $taskConfigure = @('-S', $taskRoot, '-B', $taskBuild, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Debug', '-DDUALDECK_UI_EDITOR=ON', '-DBUILD_TESTING=OFF',
        '-DCMAKE_C_COMPILER=cl.exe', '-DCMAKE_CXX_COMPILER=cl.exe',
        '-DCMAKE_C_FLAGS_DEBUG=/Od /RTC1 /Z7', '-DCMAKE_CXX_FLAGS_DEBUG=/Od /RTC1 /Z7')
    & $taskCmake @taskConfigure
    if ($LASTEXITCODE -ne 0) { throw 'Configurazione CMake non riuscita.' }
    & $taskCmake --build $taskBuild --target DualDeckJuce --parallel 3
    if ($LASTEXITCODE -ne 0) { throw 'Compilazione non riuscita.' }
    $taskBinary = Join-Path $taskBuild 'bin\DualDeckJuce.exe'
    $taskVersioned = Join-Path $taskRoot 'DualDeckJuce-UI-Debug.exe'
    Copy-Item -LiteralPath $taskBinary -Destination $taskVersioned -Force
    Write-Output ('Pronto: ' + $taskVersioned)
} finally { $env:PATH = $taskOld.PATH; $env:INCLUDE = $taskOld.INCLUDE; $env:LIB = $taskOld.LIB }
