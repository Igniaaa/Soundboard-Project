param([Parameter(Mandatory=$true)][string]$Archive)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskArchivePath = (Resolve-Path -LiteralPath $Archive).Path
$taskOutput = Join-Path $taskRoot 'test-output\keyboard-driver-audit'
$taskExtracted = Join-Path $taskOutput 'embedded'
New-Item -ItemType Directory -Path $taskExtracted -Force | Out-Null
# Static extraction only: no installer, service, device or driver is executed.
$taskResources = & python (Join-Path $taskRoot 'scripts\inspect-interception-resources.py') $taskArchivePath $taskExtracted
if ($LASTEXITCODE -ne 0) { throw 'Archivio non verificato; analisi interrotta.' }
$taskRecords = ($taskResources -join "`n") | ConvertFrom-Json
$taskDrivers = foreach ($taskRecord in $taskRecords) {
    $taskPath = Join-Path $taskExtracted $taskRecord.file
    $taskSignature = Get-AuthenticodeSignature -LiteralPath $taskPath
    [pscustomobject]@{
        Resource=$taskRecord.resource; File=$taskRecord.file
        Machine=$taskRecord.machine; SHA256=$taskRecord.sha256
        AuthenticodeStatus=$taskSignature.Status.ToString()
        Signer=$taskSignature.SignerCertificate.Subject
        Issuer=$taskSignature.SignerCertificate.Issuer
    }
}
$taskReport = [pscustomobject]@{
    Source='https://github.com/oblitum/Interception/releases/tag/v1.0.1'
    ArchiveSHA256=(Get-FileHash -LiteralPath $taskArchivePath).Hash
    Verification='Static resource extraction and Authenticode only; kernel loading, policy eligibility and hardware compatibility are NOT verified.'
    Drivers=@($taskDrivers)
}
$taskReport | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskOutput 'report.json') -Encoding utf8
Write-Output ('Analizzati ' + $taskDrivers.Count + ' driver incorporati senza eseguirli.')
Write-Output 'Firma Authenticode valida non equivale ad autorizzazione al caricamento del driver.'
Write-Output ('Report: ' + (Join-Path $taskOutput 'report.json'))
