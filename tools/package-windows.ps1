param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "verification-common.ps1")

$repoRoot = Get-RepoRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "artifacts\release"
}

$guiExecutable = Join-Path $repoRoot "build\windows-gui-release\Release\VidChopper.exe"
$cliExecutable = Join-Path $repoRoot "build\core-release\Release\VidChopperCLI.exe"
foreach ($required in @($guiExecutable, $cliExecutable)) {
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
$stageDirectory = Join-Path $OutputDirectory "VidChopper"
$zipPath = Join-Path $OutputDirectory "VidChopper-$Version-windows-x64.zip"
New-Item -ItemType Directory -Force -Path $stageDirectory | Out-Null

Copy-Item -LiteralPath $guiExecutable -Destination $stageDirectory -Force
Copy-Item -LiteralPath $cliExecutable -Destination $stageDirectory -Force
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

Compress-Archive -Path (Join-Path $stageDirectory "*") -DestinationPath $zipPath -Force
Write-Output $zipPath
