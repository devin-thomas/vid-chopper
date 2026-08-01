param(
    [switch]$CheckOnly
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "verification-common.ps1")

$repoRoot = Get-RepoRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "vcpkg.json") | ConvertFrom-Json
$baseline = [string]$manifest.'builtin-baseline'

function Write-DetectedTool {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$Remediation
    )

    $resolved = Get-Command $Command -ErrorAction SilentlyContinue
    if ($null -eq $resolved) {
        Write-Warning "${Name}: missing. $Remediation"
        return $false
    }

    Write-Host "${Name}: $($resolved.Source)"
    return $true
}

$cmakeFound = Write-DetectedTool -Name "CMake 3.28+" -Command "cmake" -Remediation "Install CMake 3.28 or newer and add it to PATH."
$nodeFound = Write-DetectedTool -Name "Node.js 22+" -Command "node" -Remediation "Install Node.js 22 LTS and add it to PATH."
$npmFound = Write-DetectedTool -Name "npm" -Command "npm" -Remediation "Install npm with Node.js 22."
$ffmpegFound = Write-DetectedTool -Name "ffmpeg" -Command "ffmpeg" -Remediation "Install ffmpeg 7.1.1 and add it to PATH."
$ffprobeFound = Write-DetectedTool -Name "ffprobe" -Command "ffprobe" -Remediation "Install ffprobe 7.1.1 and add it to PATH."

$clCommand = Get-Command "cl" -ErrorAction SilentlyContinue
$msvcFound = $null -ne $clCommand
if ($msvcFound) {
    Write-Host "MSVC Build Tools 2022: $($clCommand.Source)"
} else {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $visualStudioRoot = (& $vswhere -latest -products "*" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
        $msvcFound = -not [string]::IsNullOrWhiteSpace($visualStudioRoot)
        if ($msvcFound) {
            Write-Host "MSVC Build Tools 2022: $visualStudioRoot (use an x64 Native Tools prompt for direct cl.exe access)"
        }
    }
}
if (-not $msvcFound) {
    Write-Warning "MSVC Build Tools 2022: missing. Install Visual Studio 2022 Build Tools with Desktop development with C++."
}

if ($cmakeFound) {
    $cmakeText = (& cmake --version | Select-Object -First 1)
    if ($cmakeText -notmatch '(\d+\.\d+(?:\.\d+)?)' -or [version]$Matches[1] -lt $script:ToolVersions.CMake) {
        Write-Warning "CMake version mismatch: '$cmakeText'. Install CMake $($script:ToolVersions.CMake) or newer."
        $cmakeFound = $false
    }
}

if ($nodeFound) {
    $nodeText = (& node --version).TrimStart('v')
    if ([version]$nodeText -lt $script:ToolVersions.Node) {
        Write-Warning "Node.js version mismatch: '$nodeText'. Install Node.js $($script:ToolVersions.Node) or newer."
        $nodeFound = $false
    }
}

if ($ffmpegFound) {
    $ffmpegText = (& ffmpeg -version | Select-Object -First 1)
    if ($ffmpegText -notmatch "ffmpeg version $([regex]::Escape($script:ToolVersions.Ffmpeg))") {
        Write-Warning "ffmpeg version mismatch: '$ffmpegText'. Install ffmpeg $($script:ToolVersions.Ffmpeg)."
        $ffmpegFound = $false
    }
}
if ($ffprobeFound) {
    $ffprobeText = (& ffprobe -version | Select-Object -First 1)
    if ($ffprobeText -notmatch "ffprobe version $([regex]::Escape($script:ToolVersions.Ffmpeg))") {
        Write-Warning "ffprobe version mismatch: '$ffprobeText'. Install ffprobe $($script:ToolVersions.Ffmpeg)."
        $ffprobeFound = $false
    }
}

$qtRoot = Get-RepoQtRoot
$qtFound = -not [string]::IsNullOrWhiteSpace($qtRoot)
if ($qtFound) {
    [void](Set-RepoQtEnvironment -QtRoot $qtRoot)
    $qtVersion = (& (Join-Path $qtRoot "bin\qmake.exe") -query QT_VERSION).Trim()
    Write-Host "Qt ${qtVersion}: $qtRoot"
} else {
    Write-Warning "Qt 6.9: missing. Install the MSVC 2022 64-bit kit under C:\Qt or set Qt6_ROOT, Qt6_DIR, or QTDIR before Full/Release verification."
}

$clangFormatPath = Join-Path $repoRoot ".venv-tools\Scripts\clang-format.exe"
$clangTidyPath = Join-Path $repoRoot ".venv-tools\Scripts\clang-tidy.exe"
$clangFound = (Test-Path -LiteralPath $clangFormatPath) -and (Test-Path -LiteralPath $clangTidyPath)
if ($clangFound) {
    $formatVersion = (& $clangFormatPath --version) -join " "
    $tidyVersion = (& $clangTidyPath --version) -join " "
    $clangFound = $formatVersion -match [regex]::Escape($script:ToolVersions.Clang) -and $tidyVersion -match [regex]::Escape($script:ToolVersions.Clang)
}
if (-not $clangFound) {
    Write-Warning "Repo-local clang tools are missing or not version $($script:ToolVersions.Clang). Run tools/bootstrap.ps1."
} else {
    Write-Host "Repo-local clang-format/clang-tidy: $($script:ToolVersions.Clang)"
}

$vcpkgRoot = Join-Path $repoRoot ".vcpkg"
$vcpkgFound = Test-Path -LiteralPath (Join-Path $vcpkgRoot "vcpkg.exe")
if ($vcpkgFound) {
    $vcpkgHead = (& git -C $vcpkgRoot rev-parse HEAD 2>$null).Trim()
    $vcpkgFound = $LASTEXITCODE -eq 0 -and $vcpkgHead -eq $baseline
}
if (-not $vcpkgFound) {
    Write-Warning "Repository-local vcpkg is missing or not pinned to manifest baseline $baseline. Run tools/bootstrap.ps1."
} else {
    Write-Host "Repository-local vcpkg baseline: $baseline"
}

if ($CheckOnly) {
    $requiredFound = $cmakeFound -and $msvcFound -and $qtFound -and $nodeFound -and $npmFound -and
        $ffmpegFound -and $ffprobeFound -and $clangFound -and $vcpkgFound
    if (-not $requiredFound) {
        exit 1
    }
    exit 0
}

$python = Get-Command "python" -ErrorAction SilentlyContinue
if ($null -eq $python) {
    throw "Python 3 was not found. Install Python 3.12 and add it to PATH."
}

$venvRoot = Join-Path $repoRoot ".venv-tools"
if (-not (Test-Path -LiteralPath (Join-Path $venvRoot "Scripts\python.exe"))) {
    Invoke-RepoCommand -FilePath $python.Source -ArgumentList @("-m", "venv", $venvRoot)
}

$venvPython = Join-Path $venvRoot "Scripts\python.exe"
Invoke-RepoCommand -FilePath $venvPython -ArgumentList @(
    "-m", "pip", "install", "--disable-pip-version-check", "-r",
    (Join-Path $repoRoot "tools\verification-requirements.txt")
)

if (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot ".git"))) {
    $git = Get-RepoCommand -Name "git" -Remediation "Install Git and add it to PATH."
    Invoke-RepoCommand -FilePath $git -ArgumentList @("clone", "https://github.com/microsoft/vcpkg.git", $vcpkgRoot)
}

$vcpkgGit = Get-RepoCommand -Name "git" -Remediation "Install Git and add it to PATH."
$dirtyVcpkg = & $vcpkgGit -C $vcpkgRoot status --porcelain
if ($LASTEXITCODE -ne 0) {
    throw "Could not inspect the repository-local vcpkg checkout."
}
if ($dirtyVcpkg) {
    throw "The repository-local .vcpkg checkout has local changes. Preserve or remove them before bootstrap can pin baseline $baseline."
}

Invoke-RepoCommand -FilePath $vcpkgGit -ArgumentList @("-C", $vcpkgRoot, "fetch", "--depth", "1", "origin", $baseline)
Invoke-RepoCommand -FilePath $vcpkgGit -ArgumentList @("-C", $vcpkgRoot, "checkout", "--detach", $baseline)
Invoke-RepoCommand -FilePath (Join-Path $vcpkgRoot "bootstrap-vcpkg.bat") -ArgumentList @("-disableMetrics")
Invoke-RepoCommand -FilePath (Join-Path $vcpkgRoot "vcpkg.exe") -ArgumentList @(
    "install", "--x-manifest-root=$repoRoot", "--x-install-root=$(Join-Path $repoRoot 'vcpkg_installed')"
)

Write-Host "Bootstrap complete. Large SDKs were not installed; review the warnings above before running Full or Release verification."
