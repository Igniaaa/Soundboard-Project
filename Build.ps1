param([switch]$SkipTests, [string]$ToolRoot = $PSScriptRoot, [string]$OutputName = '')
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if ($OutputName -and ([System.IO.Path]::GetFileName($OutputName) -ne $OutputName -or [System.IO.Path]::GetExtension($OutputName) -ne '.exe')) {
    throw 'OutputName deve essere un nome di file .exe senza cartelle.'
}
$taskCmake = Join-Path $ToolRoot 'tools\cmake-4.4.4-windows-x86_64\bin\cmake.exe'
$taskCtest = Join-Path $ToolRoot 'tools\cmake-4.4.4-windows-x86_64\bin\ctest.exe'
$taskMsvcRoot = Join-Path $ToolRoot 'tools\msvc'
if (-not (Test-Path -LiteralPath $taskCmake)) { throw 'Esegui prima Setup.ps1 per scaricare gli strumenti portatili.' }
$taskMsvc = Get-ChildItem -LiteralPath (Join-Path $taskMsvcRoot 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$taskSdkRoot = Join-Path $taskMsvcRoot 'Windows Kits\10'
$taskSdk = Get-ChildItem -LiteralPath (Join-Path $taskSdkRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $taskMsvc -or -not $taskSdk) { throw 'Compilatore Microsoft o Windows SDK mancante. Esegui Setup.ps1.' }
$taskOld = @{ PATH=$env:PATH; INCLUDE=$env:INCLUDE; LIB=$env:LIB }
$taskBuild = Join-Path $taskRoot 'build-msvc'
try {
    $env:PATH = (Join-Path $taskMsvc.FullName 'bin\Hostx64\x64') + ';' + (Join-Path $taskSdkRoot ('bin\' + $taskSdk.Name + '\x64')) + ';' + (Join-Path $ToolRoot 'tools\ninja') + ';' + $env:PATH
    $taskIncludes = @((Join-Path $taskMsvc.FullName 'include'))
    foreach ($taskPart in @('ucrt','shared','um','winrt','cppwinrt')) { $taskIncludes += Join-Path $taskSdk.FullName $taskPart }
    $env:INCLUDE = $taskIncludes -join ';'
    $env:LIB = @((Join-Path $taskMsvc.FullName 'lib\x64'),(Join-Path $taskSdkRoot ('Lib\' + $taskSdk.Name + '\ucrt\x64')),(Join-Path $taskSdkRoot ('Lib\' + $taskSdk.Name + '\um\x64'))) -join ';'
    $taskConfigure = @('-S', $taskRoot, '-B', $taskBuild, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=cl.exe', '-DCMAKE_CXX_COMPILER=cl.exe')
    $taskCache = Join-Path $taskBuild 'CMakeCache.txt'
    if (Test-Path -LiteralPath $taskCache) {
        $taskCachedRoot = Get-Content -LiteralPath $taskCache | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' } | Select-Object -First 1
        $taskCachedNinja = Get-Content -LiteralPath $taskCache | Where-Object { $_ -like 'CMAKE_MAKE_PROGRAM:FILEPATH=*' } | Select-Object -First 1
        $taskNinjaMissing = $taskCachedNinja -and -not (Test-Path -LiteralPath $taskCachedNinja.Substring('CMAKE_MAKE_PROGRAM:FILEPATH='.Length))
        if (($taskCachedRoot -and $taskCachedRoot.Substring('CMAKE_HOME_DIRECTORY:INTERNAL='.Length).Replace('/', '\') -ne $taskRoot) -or $taskNinjaMissing) {
            $taskConfigure += '--fresh'
            Write-Output 'Percorso del progetto o di Ninja cambiato: rigenerazione della configurazione CMake.'
        }
    }
    & $taskCmake @taskConfigure
    if ($LASTEXITCODE -ne 0) { throw 'Configurazione CMake non riuscita.' }
    & $taskCmake --build $taskBuild --parallel 3
    if ($LASTEXITCODE -ne 0) { throw 'Compilazione non riuscita.' }
    if (-not $SkipTests) {
        & $taskCtest --test-dir $taskBuild --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'Test audio non superati.' }
    }
    $taskBinary = Join-Path $taskRoot 'build-msvc\bin\DualDeckJuce.exe'
    if ($OutputName) {
        $taskTrial = Join-Path $taskRoot $OutputName
        Copy-Item -LiteralPath $taskBinary -Destination $taskTrial -Force
        Write-Output ('Build di prova pronta: ' + $taskTrial)
        return
    }
    $taskVersioned = Join-Path $taskRoot 'DualDeckJuce-0.7.exe'
    Copy-Item -LiteralPath $taskBinary -Destination $taskVersioned -Force
    try { Copy-Item -LiteralPath $taskBinary -Destination (Join-Path $taskRoot 'DualDeckJuce.exe') -Force }
    catch { Write-Warning 'Il nome precedente non e aggiornabile (app aperta?). Chiudila e avvia DualDeckJuce-0.7.exe.' }
    Write-Output ('Pronto: ' + $taskVersioned)
} finally { $env:PATH = $taskOld.PATH; $env:INCLUDE = $taskOld.INCLUDE; $env:LIB = $taskOld.LIB }
