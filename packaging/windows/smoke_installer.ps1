param(
    [Parameter(Mandatory = $true)]
    [string]$Installer,

    [ValidateSet('BASIC', 'WEBENGINE')]
    [string]$ChatType = 'BASIC'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Loader error dialogs must not hang an unattended CI runner.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class PsiSmokeErrorMode {
    [DllImport("kernel32.dll")]
    public static extern uint SetErrorMode(uint flags);
}
'@
[void][PsiSmokeErrorMode]::SetErrorMode(0x0001 -bor 0x0002)

$installerPath = (Resolve-Path $Installer).Path
$installDir = Join-Path $env:RUNNER_TEMP ("psi-smoke-" + [guid]::NewGuid().ToString('N'))
$psi = $null

try {
    Write-Host "Installing NSIS package into: $installDir"
    # NSIS requires /D= to be the LAST argument and unquoted.
    $install = Start-Process -FilePath $installerPath -ArgumentList @('/S', "/D=$installDir") -Wait -PassThru
    if ($install.ExitCode -ne 0) {
        throw "NSIS installer failed with exit code $($install.ExitCode)"
    }

    $exe = Join-Path $installDir 'psi.exe'
    $required = @('psi.exe', 'Qt6Core.dll')
    if ($ChatType -eq 'WEBENGINE') {
        $required += @('Qt6WebEngineCore.dll', 'Qt6WebEngineWidgets.dll')
    }
    foreach ($name in $required) {
        $path = Join-Path $installDir $name
        if (-not (Test-Path $path -PathType Leaf)) {
            throw "Installed package is missing $name"
        }
        Write-Host ("{0}: {1}" -f $name,
            [System.Diagnostics.FileVersionInfo]::GetVersionInfo($path).FileVersion)
    }

    $buildInfo = Join-Path $installDir 'BUILD-INFO.txt'
    if (-not (Test-Path $buildInfo)) {
        throw 'Installed package is missing BUILD-INFO.txt'
    }
    if ((Get-Content $buildInfo -Raw) -notmatch "(?m)^Chat rendering: $ChatType\r?$") {
        throw "Installed package does not report Chat rendering: $ChatType"
    }

    Write-Host "Launching installed $ChatType Psi (without SDK or build-tree DLLs)"
    $oldPath = $env:PATH
    try {
        # Avoid accidentally resolving DLLs against the runner's Qt and SDK paths.
        $env:PATH = "$env:SystemRoot\system32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
        $psi = Start-Process -FilePath $exe -WorkingDirectory $installDir -PassThru
        if ($psi.WaitForExit(12000)) {
            $status = [int64]$psi.ExitCode
            $hex = ([uint32]($status -band 0xffffffffL)).ToString('X8')
            if ($status -ne 0) {
                throw "Installed psi.exe failed to start: exit $status (0x$hex). 0xC0000139 means a missing DLL entry point."
            }
            Write-Host 'Installed Psi exited cleanly (0).'
        } else {
            Write-Host 'Installed Psi is still running after 12 seconds: loader/startup passed.'
        }
    } finally {
        $env:PATH = $oldPath
    }
} finally {
    if ($null -ne $psi) {
        try {
            $psi.Refresh()
            if (-not $psi.HasExited) {
                Stop-Process -Id $psi.Id -Force -ErrorAction SilentlyContinue
                $psi.WaitForExit(5000) | Out-Null
            }
        } catch { Write-Warning "Psi cleanup: $_" }
    }
    $uninstaller = Join-Path $installDir 'uninstall.exe'
    if (Test-Path $uninstaller) {
        try {
            $uninstall = Start-Process -FilePath $uninstaller -ArgumentList '/S' -PassThru
            if (-not $uninstall.WaitForExit(60000)) {
                Stop-Process -Id $uninstall.Id -Force -ErrorAction SilentlyContinue
                Write-Warning 'Silent uninstaller timed out'
            } elseif ($uninstall.ExitCode -ne 0) {
                Write-Warning "Uninstaller exited with $($uninstall.ExitCode)"
            }
        } catch { Write-Warning "Uninstaller cleanup: $_" }
    }
}
