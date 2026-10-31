param([string]$Archive = '', [string]$SourceArchive = '')
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if (-not $Archive) { $Archive = Join-Path $taskRoot 'deps\Interception-v1.0.1.zip' }
if (-not $SourceArchive) { $SourceArchive = Join-Path $taskRoot 'deps\Interception-v1.0.1-source.zip' }
$taskGpl = Join-Path $taskRoot 'deps\GPL-3.0.txt'
$taskInputs = @(
    @{File=$Archive;Url='https://github.com/oblitum/Interception/releases/download/v1.0.1/Interception.zip';Hash='AD038963D6413055765128B0B931F6E765147C9916DBA79E65D872B261F9AF10'},
    @{File=$SourceArchive;Url='https://github.com/oblitum/Interception/archive/refs/tags/v1.0.1.zip';Hash='43E6A4F71085C8797DF3EA1DF40CBDA72530546F45B697944A237A1A8CD3EAA8'},
    @{File=$taskGpl;Url='https://raw.githubusercontent.com/gcc-mirror/gcc/master/COPYING3';Hash='8CEB4B9EE5ADEDDE47B31E975C1D90C73AD27B6B165A1DCD80C7C545EB65B903'}
)
foreach ($taskInput in $taskInputs) {
    if (-not (Test-Path -LiteralPath $taskInput.File)) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $taskInput.File) -Force | Out-Null
        Invoke-WebRequest -Uri $taskInput.Url -OutFile $taskInput.File
    }
    if ((Get-FileHash -LiteralPath $taskInput.File).Hash -ne $taskInput.Hash) { throw ('Hash inatteso: ' + $taskInput.File) }
}
$taskBinarySource = Join-Path $taskRoot 'deps\interception-runtime-package'
$taskLibrarySource = Join-Path $taskRoot 'deps\interception-source-v1.0.1'
Expand-Archive -LiteralPath $Archive -DestinationPath $taskBinarySource -Force
Expand-Archive -LiteralPath $SourceArchive -DestinationPath $taskLibrarySource -Force
$taskDll = Join-Path $taskBinarySource 'Interception\library\x64\interception.dll'
if ((Get-FileHash -LiteralPath $taskDll).Hash -ne 'AB88164C11B1B48488772D4C3BFAA4509D5B0AE9DBC5A691DC4F96F0260443C8') { throw 'DLL x64 non verificata.' }
$taskRuntime = Join-Path $taskRoot 'keyboard-runtime'
New-Item -ItemType Directory -Path $taskRuntime -Force | Out-Null
Copy-Item -LiteralPath $taskDll -Destination (Join-Path $taskRuntime 'interception.dll') -Force
Copy-Item -LiteralPath (Join-Path $taskLibrarySource 'Interception-1.0.1\licenses\non-commercial-usage\LGPL 3.0.txt') -Destination (Join-Path $taskRuntime 'LGPL-3.0.txt') -Force
Copy-Item -LiteralPath $taskGpl -Destination (Join-Path $taskRuntime 'GPL-3.0.txt') -Force
Copy-Item -LiteralPath $SourceArchive -Destination (Join-Path $taskRuntime 'Interception-v1.0.1-source.zip') -Force
@'
Interception user-mode library, unmodified x64 release v1.0.1.
Author: Francisco Lopes da Silva (oblitum).
Project and license information: https://github.com/oblitum/Interception
Official release: https://github.com/oblitum/Interception/releases/tag/v1.0.1
The library is dynamically loaded through its published C API.
For filter writes, DualDeck also uses the pinned x64 v1.0.1 context layout
and SET_FILTER IOCTL from library/interception.c to retain the driver
acknowledgement; the installed legacy driver does not implement GET_FILTER.
LGPL-3.0.txt and GPL-3.0.txt are included, together with the unmodified
source archive for tag v1.0.1. See its README and licenses directory.
This directory does not install, start, or redistribute kernel-driver binaries.
The author's free driver distribution is described for non-commercial use.
DualDeck source contains the default trusted DLL SHA-256; a replacement
library can be used by adapting that value and rebuilding the app.
'@ | Set-Content -LiteralPath (Join-Path $taskRuntime 'THIRD-PARTY-NOTICES.txt') -Encoding utf8
Write-Output ('Runtime tastiera pronto: ' + $taskRuntime)
Write-Output 'Preparata solo la libreria: nessun driver installato o modificato.'
