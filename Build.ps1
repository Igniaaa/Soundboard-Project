param(
    [switch]$SkipTests, [string]$ToolRoot = $PSScriptRoot, [string]$OutputName = '',
    [switch]$NoPromote, [string]$BuildDirectory = '', [string]$JuceDirectory = '',
    [ValidateSet('Auto','Portable','Installed')][string]$Toolchain = 'Auto'
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if ($OutputName -and ([System.IO.Path]::GetFileName($OutputName) -ne $OutputName -or [System.IO.Path]::GetExtension($OutputName) -ne '.exe')) {
    throw 'OutputName deve essere un nome di file .exe senza cartelle.'
}
$taskCmake = Join-Path $ToolRoot 'tools\cmake-4.4.4-windows-x86_64\bin\cmake.exe'
$taskCtest = Join-Path $ToolRoot 'tools\cmake-4.4.4-windows-x86_64\bin\ctest.exe'
$taskMsvcRoot = Join-Path $ToolRoot 'tools\msvc'
if (-not (Test-Path -LiteralPath $taskCmake)) { throw 'Esegui prima Setup.ps1 per scaricare gli strumenti portatili.' }
$taskOld = @{ PATH=$env:PATH; INCLUDE=$env:INCLUDE; LIB=$env:LIB }
$taskBuild = if ($BuildDirectory) { [System.IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $taskRoot 'build-msvc' }
$taskVersion = (Get-Content -LiteralPath (Join-Path $taskRoot 'VERSION') -Raw).Trim()
if ($taskVersion -notmatch '^\d+\.\d+\.\d+-rc[1-9]\d*$') { throw 'VERSION non identifica una candidata valida.' }
$taskPortable = $Toolchain -eq 'Portable' -or ($Toolchain -eq 'Auto' -and (Test-Path -LiteralPath (Join-Path $taskMsvcRoot 'VC\Tools\MSVC')))
$taskReleaseScript = Join-Path $taskRoot 'scripts\release.py'
$taskJuce = if ($JuceDirectory) { [System.IO.Path]::GetFullPath($JuceDirectory) } else { Join-Path $taskRoot 'deps\JUCE-9.0.4' }
& python $taskReleaseScript verify-juce --root $taskRoot --juce-directory $taskJuce
if ($LASTEXITCODE -ne 0) { throw 'I sorgenti JUCE non corrispondono alla dipendenza distribuibile.' }
$taskSourceFingerprint = & python $taskReleaseScript fingerprint --root $taskRoot
if ($LASTEXITCODE -ne 0) { throw 'Impossibile verificare lo snapshot dei sorgenti.' }
try {
  if ($taskPortable) {
    $taskMsvc = Get-ChildItem -LiteralPath (Join-Path $taskMsvcRoot 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
    $taskSdkRoot = Join-Path $taskMsvcRoot 'Windows Kits\10'
    $taskSdk = Get-ChildItem -LiteralPath (Join-Path $taskSdkRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if (-not $taskMsvc -or -not $taskSdk) { throw 'Toolchain portatile incompleta. Vedi BUILDING.md.' }
    $env:PATH = (Join-Path $taskMsvc.FullName 'bin\Hostx64\x64') + ';' + (Join-Path $taskSdkRoot ('bin\' + $taskSdk.Name + '\x64')) + ';' + (Join-Path $ToolRoot 'tools\ninja') + ';' + $env:PATH
    $taskIncludes = @((Join-Path $taskMsvc.FullName 'include'))
    foreach ($taskPart in @('ucrt','shared','um','winrt','cppwinrt')) { $taskIncludes += Join-Path $taskSdk.FullName $taskPart }
    $env:INCLUDE = $taskIncludes -join ';'
    $env:LIB = @((Join-Path $taskMsvc.FullName 'lib\x64'),(Join-Path $taskSdkRoot ('Lib\' + $taskSdk.Name + '\ucrt\x64')),(Join-Path $taskSdkRoot ('Lib\' + $taskSdk.Name + '\um\x64'))) -join ';'
    $taskConfigure = @('-S', $taskRoot, '-B', $taskBuild, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=cl.exe', '-DCMAKE_CXX_COMPILER=cl.exe')
    $taskGenerator = 'Ninja'
  } else {
    $taskGenerator = 'Visual Studio 17 2022'
    $taskConfigure = @('-S', $taskRoot, '-B', $taskBuild, '-G', $taskGenerator, '-A', 'x64')
  }
    $taskConfigure += '-DJUCE_DIR=' + $taskJuce
    $taskCache = Join-Path $taskBuild 'CMakeCache.txt'
    if (Test-Path -LiteralPath $taskCache) {
        $taskCachedRoot = Get-Content -LiteralPath $taskCache | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' } | Select-Object -First 1
        $taskCachedNinja = Get-Content -LiteralPath $taskCache | Where-Object { $_ -like 'CMAKE_MAKE_PROGRAM:FILEPATH=*' } | Select-Object -First 1
        $taskNinjaMissing = $taskCachedNinja -and -not (Test-Path -LiteralPath $taskCachedNinja.Substring('CMAKE_MAKE_PROGRAM:FILEPATH='.Length))
        $taskCachedGenerator = Get-Content -LiteralPath $taskCache | Where-Object { $_ -like 'CMAKE_GENERATOR:INTERNAL=*' } | Select-Object -First 1
        $taskGeneratorChanged = $taskCachedGenerator -and $taskCachedGenerator.Substring('CMAKE_GENERATOR:INTERNAL='.Length) -ne $taskGenerator
        if (($taskCachedRoot -and $taskCachedRoot.Substring('CMAKE_HOME_DIRECTORY:INTERNAL='.Length).Replace('/', '\') -ne $taskRoot) -or $taskNinjaMissing -or $taskGeneratorChanged) {
            $taskConfigure += '--fresh'
            Write-Output 'Percorso del progetto o di Ninja cambiato: rigenerazione della configurazione CMake.'
        }
    }
    & $taskCmake @taskConfigure
    if ($LASTEXITCODE -ne 0) { throw 'Configurazione CMake non riuscita.' }
    & $taskCmake --build $taskBuild --config Release --parallel 3
    if ($LASTEXITCODE -ne 0) { throw 'Compilazione non riuscita.' }
    if (-not $SkipTests) {
        & $taskCtest --test-dir $taskBuild -C Release --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'Test audio non superati.' }
    }
    $taskBinary = Join-Path $taskBuild 'bin\DualDeckJuce.exe'
    & python $taskReleaseScript stamp --root $taskRoot --binary $taskBinary --expected-source $taskSourceFingerprint --juce-directory $taskJuce
    if ($LASTEXITCODE -ne 0) { throw 'Sorgenti cambiati durante la build: ricompilare prima di distribuire.' }
    if ($NoPromote) { Write-Output ('Build pronta, copie approvate invariate: ' + $taskBinary); return }
    if ($OutputName) {
        $taskTrial = Join-Path $taskRoot $OutputName
        Copy-Item -LiteralPath $taskBinary -Destination $taskTrial -Force
        Write-Output ('Build di prova pronta: ' + $taskTrial)
        return
    }
    $taskVersioned = Join-Path $taskRoot ('DualDeckJuce-' + $taskVersion + '.exe')
    Copy-Item -LiteralPath $taskBinary -Destination $taskVersioned -Force
    # La candidata non sostituisce gli eseguibili approvati prima della prova utente.
    Write-Output ('Pronto: ' + $taskVersioned)
} finally { $env:PATH = $taskOld.PATH; $env:INCLUDE = $taskOld.INCLUDE; $env:LIB = $taskOld.LIB }
