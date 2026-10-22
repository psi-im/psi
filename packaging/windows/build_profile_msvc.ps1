param(
    [Parameter(Mandatory = $true)]
    [string]$SdkDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [ValidateSet('BASIC', 'WEBENGINE')]
    [string]$ChatType = 'BASIC',

    [switch]$RunTests
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
if (-not $env:PSI_L10N_REF -or $env:PSI_L10N_REF -notmatch '^[a-f0-9]{40}$') {
    throw 'Expected an exact Psi translations SHA from dependencies.lock.json'
}
Invoke-Checked git -C $l10nDir fetch --depth 1 origin $env:PSI_L10N_REF
Invoke-Checked git -C $l10nDir checkout --detach $env:PSI_L10N_REF
$l10nRevision = (git -C $l10nDir rev-parse HEAD).Trim()
if ($l10nRevision -ne $env:PSI_L10N_REF) { throw 'Psi translations checkout differs from lock' }

$qtKeychainDir = Join-Path $sdkDir 'lib\cmake\Qt6Keychain'
if (-not (Test-Path (Join-Path $qtKeychainDir 'Qt6KeychainConfig.cmake'))) {
    throw "Qt6Keychain SDK is missing: $qtKeychainDir"
}
# Psi and its dynamically loaded plugins (OTR) must share one QCA instance,
# so QCA comes from the SDK as a shared library, never bundled into Psi.
$qcaDir = Join-Path $sdkDir 'lib\cmake\Qca3-qt6'
if (-not (Test-Path (Join-Path $qcaDir 'Qca3-qt6Config.cmake'))) {
    throw "Shared QCA SDK is missing: $qcaDir"
}
$qcaPluginPath = Join-Path $sdkDir 'lib\qca3-qt6'
if (-not (Test-Path (Join-Path $qcaPluginPath 'crypto\qca-ossl.dll'))) {
    throw "QCA OpenSSL provider is missing from the SDK: $qcaPluginPath"
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
    "-DQca3-qt6_DIR=$qcaDir",
    "-DTRANSLATIONS_DIR=$l10nDir\translations",
    '-DCMAKE_C_FLAGS=/D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00',
    '-DCMAKE_CXX_FLAGS=/D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00',
    '-DQT_DEFAULT_MAJOR_VERSION=6',
    '-DUSE_QT6=ON',
    '-DUSE_MXE=OFF',
    '-DUSE_CCACHE=OFF',
    '-DCMAKE_C_COMPILER_LAUNCHER=ccache',
    '-DCMAKE_CXX_COMPILER_LAUNCHER=ccache',
    "-DCHAT_TYPE=$ChatType",
    '-DBUNDLED_IRIS=ON',
    '-DBUNDLED_IRIS_ALL=OFF',
    '-DIRIS_BUNDLED_QCA=OFF',
    '-DIRIS_SYSTEM_QCA=3',
    # Not in the MSVC SDK and not shared with plugins: keep them in Iris.
    '-DIRIS_BUNDLED_USRSCTP=ON',
    '-DIRIS_BUNDLED_OMEMO_C=ON',
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
    $(if ($ChatType -eq 'WEBENGINE') { 'Chatlog type - QtWebEngine' } else { 'Chatlog type - Basic' }),
    'QCA: selected system QCA 3',
    'Psi SDK Found at',
    'Qt6 found'
)) {
    if ($configureText -notmatch [regex]::Escape($required)) {
        throw "Expected configure marker was not found: $required"
    }
}
$cacheText = Get-Content (Join-Path $buildDir 'CMakeCache.txt') -Raw
foreach ($required in @(
    'IRIS_BUNDLED_QCA:BOOL=OFF',
    'IRIS_BUNDLED_USRSCTP:BOOL=ON',
    'IRIS_BUNDLED_OMEMO_C:BOOL=ON'
)) {
    if ($cacheText -notmatch "(?m)^$([regex]::Escape($required))\r?$") {
        throw "Expected CMake cache entry was not found: $required"
    }
}

Invoke-Checked cmake --build $buildDir --parallel 4
if ($RunTests) {
    $testingArg = [Array]::IndexOf($configureArgs, '-DBUILD_TESTING=OFF')
    if ($testingArg -lt 0) {
        throw 'BUILD_TESTING configure argument is missing'
    }
    $configureArgs[$testingArg] = '-DBUILD_TESTING=ON'
    Invoke-Checked cmake @configureArgs
    Invoke-Checked cmake --build $buildDir --target avcallpolicy_test avcall_backend_lifecycle_test avcall_capability_refresh_test --parallel 4
    $testPath = "$(Join-Path $env:QT_ROOT_DIR 'bin');$(Join-Path $sdkDir 'bin');$env:PATH"
    # The test binaries run from the build tree; point QCA at the SDK provider
    # and fail instead of skipping when DTLS-SRTP is unavailable.
    $testArgs = @(
        '-E', 'env', "PATH=$testPath", "QCA_PLUGIN_PATH=$qcaPluginPath", 'PSI_TEST_REQUIRE_DTLS=1', 'ctest',
        '--test-dir', $buildDir,
        '--output-on-failure',
        '-R', '^avcall(policy|_(backend_lifecycle|capability_refresh))_test$',
        '-j', '1'
    )
    & cmake @testArgs
    if ($LASTEXITCODE -ne 0) {
        $ctestStatus = $LASTEXITCODE
        # QTest's console output can be lost on Windows; write it to a file
        # and report the raw exit status (an NTSTATUS for crashes).
        $previousPath = $env:PATH
        $env:PATH = $testPath
        $env:QCA_PLUGIN_PATH = $qcaPluginPath
        $env:PSI_TEST_REQUIRE_DTLS = '1'
        $env:QT_QPA_PLATFORM = 'offscreen'
        foreach ($name in @('avcallpolicy_test', 'avcall_backend_lifecycle_test', 'avcall_capability_refresh_test')) {
            $exe = Get-ChildItem -Path $buildDir -Recurse -Filter "$name.exe" | Select-Object -First 1
            if (-not $exe) { continue }
            $log = Join-Path $buildDir "$name-qtest.txt"
            & $exe.FullName -o "$log,txt"
            $status = $LASTEXITCODE
            if ($status -ne 0) {
                Write-Host ("{0} exited with {1} (0x{2:X8})" -f $name, $status, $status)
                if (Test-Path $log) { Get-Content $log }
            }
        }
        $env:PATH = $previousPath
        throw "AvCall regression tests failed with exit code $ctestStatus"
    }
}
Invoke-Checked cmake --install $buildDir

# Both extension projects were built and released independently. Stage their
# verified binaries and GStreamer runtime, without rebuilding plugin sources.
Invoke-Checked python packaging/windows/stage_released_extensions.py `
    --root $stageDir `
    --plugins (Join-Path $sourceDir 'released-plugins') `
    --media (Join-Path $sourceDir 'released-media') `
    --lock (Join-Path $sourceDir 'dependencies.lock.json')

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
Qt: $qtVersion
Chat rendering: $ChatType
Translations: psi-im/psi-l10n $l10nRevision
"@ | Set-Content (Join-Path $stageDir 'BUILD-INFO.txt') -Encoding utf8

$packageBase = "psi-$appVersion-win10-x64-$($ChatType.ToLowerInvariant())"
$zipFile = Join-Path $outputDir "$packageBase.zip"
$setupFile = Join-Path $outputDir "$packageBase-setup.exe"
$manifest = Join-Path $outputDir 'uninstall-modern.nsh'

Invoke-Checked python packaging/windows/generate_nsis_manifest.py `
    --root $stageDir `
    --output $manifest

Remove-Item $zipFile, $setupFile -Force -ErrorAction SilentlyContinue
Push-Location $stageDir
try {
    Invoke-Checked cmake '-E' tar cf $zipFile --format=zip .
} finally {
    Pop-Location
}
Invoke-Checked cmake '-E' tar tf $zipFile

$makensis = (Get-Command makensis.exe -ErrorAction Stop).Source
$nsisArgs = @(
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
