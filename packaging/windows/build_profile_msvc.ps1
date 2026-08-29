param(
    [Parameter(Mandatory = $true)]
    [string]$SdkDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

$sourceDir = (Get-Location).Path
$sdkDir = (Resolve-Path $SdkDir).Path
New-Item -ItemType Directory -Force $OutputDir | Out-Null
$outputDir = (Resolve-Path $OutputDir).Path

$ccache = (Get-Command ccache.exe -ErrorAction Stop).Source
$ccacheDir = Join-Path $sourceDir '.ccache'
New-Item -ItemType Directory -Force $ccacheDir | Out-Null
$env:CCACHE_DIR = $ccacheDir
Invoke-Checked $ccache --max-size=1G
Invoke-Checked $ccache --zero-stats

$buildDir = Join-Path $sourceDir 'build-msvc-modern'
$stageDir = Join-Path $sourceDir 'package-msvc-modern'
$l10nDir = Join-Path $sourceDir '.packaging-psi-l10n-msvc-modern'
$report = Join-Path $outputDir 'dependencies-modern.txt'

Remove-Item $buildDir, $stageDir, $l10nDir -Recurse -Force -ErrorAction SilentlyContinue

# Psi translations live in psi-l10n. Keep the release package independent of
# whatever translation checkout happens to exist in the source workspace.
Invoke-Checked git clone --depth 1 --filter=blob:none --sparse `
    https://github.com/psi-im/psi-l10n.git $l10nDir
Invoke-Checked git -C $l10nDir sparse-checkout set translations
$l10nRevision = (git -C $l10nDir rev-parse HEAD).Trim()

$qtKeychainDir = Join-Path $sdkDir 'lib\cmake\Qt6Keychain'
if (-not (Test-Path (Join-Path $qtKeychainDir 'Qt6KeychainConfig.cmake'))) {
    throw "Qt6Keychain SDK is missing: $qtKeychainDir"
}

$configureLog = Join-Path $outputDir 'configure-modern.log'
$configureArgs = @(
    '-S', '.',
    '-B', $buildDir,
    '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_INSTALL_PREFIX=$stageDir",
    "-DCMAKE_PREFIX_PATH=$sdkDir;$env:QT_ROOT_DIR",
    "-DSDK_PATH=$sdkDir",
    "-DQt6Keychain_DIR=$qtKeychainDir",
    "-DTRANSLATIONS_DIR=$l10nDir\translations",
    '-DCMAKE_C_FLAGS=/D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00',
    '-DCMAKE_CXX_FLAGS=/D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00',
    '-DQT_DEFAULT_MAJOR_VERSION=6',
    '-DUSE_QT6=ON',
    '-DUSE_MXE=OFF',
    '-DUSE_CCACHE=OFF',
    '-DCMAKE_C_COMPILER_LAUNCHER=ccache',
    '-DCMAKE_CXX_COMPILER_LAUNCHER=ccache',
    '-DCHAT_TYPE=WEBENGINE',
    '-DBUNDLED_IRIS=ON',
    '-DBUNDLED_IRIS_ALL=ON',
    '-DIRIS_BUNDLED_QCA_GIT_TAG=v3.0.1',
    '-DENABLE_OMEMO=ON',
    '-DUSE_HUNSPELL=ON',
    '-DUSE_KEYCHAIN=ON',
    '-DBUNDLED_KEYCHAIN=OFF',
    "-DOPENSSL_ROOT_DIR=$sdkDir",
    '-DOPENSSL_USE_STATIC_LIBS=OFF',
    "-DHUNSPELL_ROOT=$sdkDir",
    "-DMINIZIP_ROOT=$sdkDir",
    '-DBUILD_TESTING=OFF',
    '-DENABLE_PLUGINS=OFF',
    '-DBUILD_PSIMEDIA=OFF',
    '-DONLY_BINARY=OFF',
    '-DINSTALL_EXTRA_FILES=ON'
)

& cmake @configureArgs 2>&1 | Tee-Object -FilePath $configureLog
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

$configureText = Get-Content $configureLog -Raw
foreach ($required in @(
    'Chatlog type - QtWebEngine',
    'BUNDLED_IRIS_ALL - ENABLED',
    'Psi SDK Found at',
    'Qt6 found'
)) {
    if ($configureText -notmatch [regex]::Escape($required)) {
        throw "Expected configure marker was not found: $required"
    }
}

Invoke-Checked cmake --build $buildDir --parallel 4
Invoke-Checked cmake --install $buildDir

$psiExe = Join-Path $stageDir 'psi.exe'
if (-not (Test-Path $psiExe)) {
    throw "Installed Psi executable is missing: $psiExe"
}

$translation = Get-ChildItem (Join-Path $stageDir 'translations') -Filter 'psi_*.qm' -File -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $translation) {
    throw 'No installed Psi translations were found'
}

Copy-Item (Join-Path $sourceDir 'COPYING') (Join-Path $stageDir 'COPYING')
Copy-Item (Join-Path $sourceDir 'README.html') (Join-Path $stageDir 'README.html')

Invoke-Checked python packaging/windows/prepare_package_msvc.py `
    --root $stageDir `
    --sdk $sdkDir `
    --qt-root $env:QT_ROOT_DIR `
    --source-root $sourceDir `
    --qt-conf (Join-Path $sourceDir 'win32\qt.conf') `
    --report $report

$baseTag = (git describe --tags --abbrev=0).Trim()
if (-not $baseTag) {
    throw 'Could not determine the base Psi version tag'
}
$commitCountText = (git rev-list --count "$baseTag..HEAD").Trim()
$commitCount = [int]$commitCountText
$appVersion = if ($commitCount -gt 0) { "$baseTag.$commitCount" } else { $baseTag }

$parts = @($appVersion.Split('.'))
if ($parts.Count -eq 0 -or ($parts | Where-Object { $_ -notmatch '^\d+$' })) {
    throw "NSIS requires a numeric application version, got: $appVersion"
}
while ($parts.Count -lt 4) {
    $parts += '0'
}
if ($parts.Count -gt 4) {
    $parts = $parts[0..3]
}
foreach ($part in $parts) {
    if ([int]$part -gt 65535) {
        throw "NSIS version component exceeds 65535: $appVersion"
    }
}
$numericVersion = $parts -join '.'

$psiRevision = (git rev-parse HEAD).Trim()
$qtVersion = (& "$env:QT_ROOT_DIR\bin\qmake.exe" -query QT_VERSION).Trim()
@"
Psi Windows package
===================
Psi version: $appVersion
Psi revision: $psiRevision
Windows profile: modern
Compiler: MSVC $env:VCToolsVersion
Target baseline: Windows 10 (_WIN32_WINNT=0x0A00, WINVER=0x0A00)
Qt: $qtVersion / QtWebEngine
Translations: psi-im/psi-l10n $l10nRevision
"@ | Set-Content (Join-Path $stageDir 'BUILD-INFO.txt') -Encoding utf8

$packageBase = "psi-$appVersion-win10-x64"
$zipFile = Join-Path $outputDir "$packageBase.zip"
$setupFile = Join-Path $outputDir "$packageBase-setup.exe"
$manifest = Join-Path $outputDir 'uninstall-modern.nsh'

Invoke-Checked python packaging/windows/generate_nsis_manifest.py `
    --root $stageDir `
    --output $manifest

$needsVcRedist = $false
$lines = Get-Content $report
$start = [Array]::IndexOf($lines, 'MSVC runtime imports:')
if ($start -lt 0) {
    throw 'Dependency report has no MSVC runtime section'
}
for ($i = $start + 1; $i -lt $lines.Count -and $lines[$i].Trim(); ++$i) {
    if ($lines[$i].Trim() -ne 'none') {
        $needsVcRedist = $true
        break
    }
}

$nsisRedistArg = $null
if ($needsVcRedist) {
    $redist = Join-Path $outputDir 'vc_redist-modern.x64.exe'
    Invoke-WebRequest `
        -Uri 'https://aka.ms/vc14/vc_redist.x64.exe' `
        -OutFile $redist `
        -MaximumRetryCount 3 `
        -RetryIntervalSec 3
    if (-not (Test-Path $redist) -or (Get-Item $redist).Length -eq 0) {
        throw 'Microsoft Visual C++ Redistributable download failed'
    }
    $nsisRedistArg = "-DVC_REDIST=$redist"
}

Remove-Item $zipFile, $setupFile -Force -ErrorAction SilentlyContinue
Push-Location $stageDir
try {
    Invoke-Checked cmake -E tar cf $zipFile --format=zip .
} finally {
    Pop-Location
}
Invoke-Checked cmake -E tar tf $zipFile

$makensis = (Get-Command makensis.exe -ErrorAction Stop).Source
$nsisArgs = @()
if ($nsisRedistArg) {
    $nsisArgs += $nsisRedistArg
}
$nsisArgs += @(
    "-DAPP_VERSION=$appVersion",
    "-DAPP_VERSION_NUMERIC=$numericVersion",
    "-DSOURCE_DIR=$stageDir",
    "-DUNINSTALL_MANIFEST=$manifest",
    "-DOUTPUT_FILE=$setupFile",
    "-DAPP_ICON=$(Join-Path $sourceDir 'win32\app.ico')",
    "-DLICENSE_FILE=$(Join-Path $sourceDir 'COPYING')",
    (Join-Path $sourceDir 'packaging\windows\psi.nsi')
)

Invoke-Checked $makensis @nsisArgs

if (-not (Test-Path $setupFile) -or (Get-Item $setupFile).Length -eq 0) {
    throw "Installer was not created: $setupFile"
}

Set-Content (Join-Path $outputDir 'version-modern.txt') $appVersion -Encoding ascii
Write-Host "Created $zipFile"
Write-Host "Created $setupFile"
