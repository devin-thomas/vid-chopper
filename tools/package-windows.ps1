param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "verification-common.ps1")

$repoRoot = Get-RepoRoot
# The CLI agent skill has its own stable version stream. It is bundled with each
# application release, but its immutable artifact version does not follow the app.
$skillVersion = "1.0.0"
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "artifacts\release"
}

$guiExecutable = Join-Path $repoRoot "build\windows-gui-release\Release\VidChopper.exe"
$cliExecutable = Join-Path $repoRoot "build\core-release\Release\VidChopperCLI.exe"
$cliDependency = Join-Path $repoRoot "build\core-release\Release\yaml-cpp.dll"
foreach ($required in @($guiExecutable, $cliExecutable, $cliDependency)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required release executable was not found: $required"
    }
}

[void](Set-RepoQtEnvironment)
if ([string]::IsNullOrWhiteSpace($env:VCINSTALLDIR)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $visualStudioRoot = (& $vswhere -latest -products "*" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
        if (-not [string]::IsNullOrWhiteSpace($visualStudioRoot)) {
            $env:VCINSTALLDIR = Join-Path $visualStudioRoot "VC"
        }
    }
}
if ([string]::IsNullOrWhiteSpace($env:VCINSTALLDIR)) {
    throw "Visual Studio C++ tools were not found. Install the MSVC 2022 C++ workload or run from an x64 Native Tools prompt."
}

$windeployqt = Get-RepoCommand -Name "windeployqt" -Remediation "Add the Qt 6.9 bin directory to PATH."
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$stageDirectory = Join-Path $OutputDirectory "VidChopper"
$zipPath = Join-Path $OutputDirectory "VidChopper-$Version-windows-x64.zip"
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
if (Test-Path -LiteralPath $stageDirectory) {
    if ([IO.Path]::GetFullPath((Split-Path -Parent $stageDirectory)) -ne $OutputDirectory) {
        throw "Refusing to clear a package stage outside the requested output directory: $stageDirectory"
    }
    Remove-Item -LiteralPath $stageDirectory -Recurse -Force
}
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
New-Item -ItemType Directory -Force -Path $stageDirectory | Out-Null

Copy-Item -LiteralPath $guiExecutable -Destination $stageDirectory -Force
Copy-Item -LiteralPath $cliExecutable -Destination $stageDirectory -Force
Copy-Item -LiteralPath $cliDependency -Destination $stageDirectory -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "packaging\windows\README.txt") -Destination (Join-Path $stageDirectory "README.txt") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "packaging\windows\THIRD_PARTY_NOTICES.txt") -Destination $stageDirectory -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $stageDirectory -Force

Invoke-RepoCommand -FilePath $windeployqt -ArgumentList @(
    "--release", "--compiler-runtime", "--no-translations", "--dir", $stageDirectory,
    (Join-Path $stageDirectory "VidChopper.exe")
)

foreach ($runtimeFile in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "vc_redist.x64.exe")) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageDirectory $runtimeFile))) {
        throw "windeployqt did not deploy required runtime file $runtimeFile."
    }
}

$packagedCli = Join-Path $stageDirectory "VidChopperCLI.exe"
$helpOutput = (& $packagedCli --help) -join "`n"
if ($LASTEXITCODE -ne 0 -or $helpOutput -notmatch "Usage:") {
    throw "Packaged VidChopperCLI.exe help smoke test failed."
}
$versionOutput = (& $packagedCli --version) -join "`n"
if ($LASTEXITCODE -ne 0 -or $versionOutput.Trim() -ne "VidChopperCLI $Version") {
    throw "Packaged CLI version mismatch. Expected 'VidChopperCLI $Version', got '$($versionOutput.Trim())'."
}

& (Join-Path $repoRoot "tools\agent-skill-artifacts.ps1") -Mode Check
if ($LASTEXITCODE -ne 0) {
    throw "Agent skill contract validation failed."
}
$skillArtifactRoot = Join-Path $repoRoot "packaging\releases\agent-skills\v$skillVersion"
$skillArchive = Join-Path $skillArtifactRoot "vidchopper-cli.zip"
$skillManifest = Join-Path $skillArtifactRoot "manifest.json"
if (-not (Test-Path -LiteralPath $skillArchive -PathType Leaf) -or
    -not (Test-Path -LiteralPath $skillManifest -PathType Leaf)) {
    throw "Stable agent skill artifacts do not match skill version $skillVersion."
}
$packagedSkillRoot = Join-Path $stageDirectory ".agents\skills\vidchopper-cli"
New-Item -ItemType Directory -Force -Path $packagedSkillRoot | Out-Null
Expand-Archive -LiteralPath $skillArchive -DestinationPath $packagedSkillRoot
Copy-Item -LiteralPath $skillManifest `
    -Destination (Join-Path $stageDirectory ".agents\skills\vidchopper-cli.manifest.json") -Force

Compress-Archive -Path (Join-Path $stageDirectory "*") -DestinationPath $zipPath -Force
Write-Output $zipPath
