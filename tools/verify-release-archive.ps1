param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$ArchivePath,
    [string]$EvidencePath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$archive = [IO.Path]::GetFullPath($ArchivePath)
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
    throw "Release archive was not found: $archive"
}

$temporaryRoot = if ([string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    Join-Path $repoRoot "artifacts\release-smoke"
} else {
    Join-Path $env:RUNNER_TEMP "vidchopper-release-smoke"
}
$workspace = Join-Path $temporaryRoot ([Guid]::NewGuid().ToString("N"))
$packageRoot = Join-Path $workspace "package"
$isolatedCliRoot = Join-Path $workspace "isolated-cli"
$mediaRoot = Join-Path $workspace "media"
New-Item -ItemType Directory -Force -Path $packageRoot, $isolatedCliRoot, $mediaRoot | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $packageRoot

$requiredFiles = @(
    "VidChopper.exe",
    "VidChopperCLI.exe",
    "yaml-cpp.dll",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "vc_redist.x64.exe",
    "README.txt",
    "THIRD_PARTY_NOTICES.txt",
    "LICENSE"
)
foreach ($requiredFile in $requiredFiles) {
    $requiredPath = Join-Path $packageRoot $requiredFile
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Release archive is missing required file: $requiredFile"
    }
}

$bundledMediaTools = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
    Where-Object { $_.Name -in @("ffmpeg.exe", "ffprobe.exe") })
if ($bundledMediaTools.Count -ne 0) {
    throw "Release archive must not bundle ffmpeg or ffprobe."
}

Copy-Item -LiteralPath (Join-Path $packageRoot "VidChopperCLI.exe") -Destination $isolatedCliRoot
Copy-Item -LiteralPath (Join-Path $packageRoot "yaml-cpp.dll") -Destination $isolatedCliRoot
$cli = Join-Path $isolatedCliRoot "VidChopperCLI.exe"

$helpOutput = (& $cli --help) -join "`n"
if ($LASTEXITCODE -ne 0 -or $helpOutput -notmatch "Usage:") {
    throw "Isolated packaged CLI help check failed."
}
$versionOutput = ((& $cli --version) -join "`n").Trim()
if ($LASTEXITCODE -ne 0 -or $versionOutput -ne "VidChopperCLI $Version") {
    throw "Isolated packaged CLI version mismatch: $versionOutput"
}

foreach ($toolName in @("ffmpeg", "ffprobe")) {
    if ($null -eq (Get-Command $toolName -ErrorAction SilentlyContinue)) {
        throw "$toolName must be on PATH for the release archive smoke test."
    }
}

$sourceVideo = Join-Path $mediaRoot "tns-2xko-36-synthetic.mp4"
& ffmpeg -hide_banner -loglevel error -y -f lavfi -i "color=c=black:s=160x90:r=1" -t 11392 `
    -c:v libx264 -preset ultrafast -crf 51 -pix_fmt yuv420p -an $sourceVideo
if ($LASTEXITCODE -ne 0) {
    throw "Failed to create the deterministic release smoke source."
}

$fixture = Join-Path $repoRoot "tests\fixtures\chapterbuilder\tns-2xko-36-chapters.json"
$dryRunOutput = (& $cli $sourceVideo $fixture --dry-run 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $dryRunOutput -notmatch "Planned chapters: 16") {
    throw "Packaged CLI did not plan all 16 ChapterBuilder chapters."
}
$chopOutput = (& $cli chop $sourceVideo $fixture --dry-run 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $chopOutput -notmatch "Planned chapters: 16") {
    throw "Packaged CLI chop mode did not plan all 16 ChapterBuilder chapters."
}

$exportOutput = (& $cli $sourceVideo $fixture --preset ultrafast --crf 51 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $exportOutput -notmatch "Summary: exported=16") {
    throw "Packaged CLI did not export all 16 ChapterBuilder chapters."
}

$outputDirectory = Join-Path $mediaRoot "tns-2xko-36-synthetic_chapters"
$manifestPath = Join-Path $outputDirectory "vidchopper-manifest.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Packaged CLI did not write its JSON manifest."
}
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if ($manifest.jobStatus -ne "success" -or $manifest.segments.Count -ne 16) {
    throw "Packaged CLI manifest did not record 16 successful segments."
}
$failedSegments = @($manifest.segments | Where-Object { $_.processState -ne "success" })
if ($failedSegments.Count -ne 0) {
    throw "Packaged CLI manifest contains failed segments."
}
$clips = @(Get-ChildItem -LiteralPath $outputDirectory -Filter "*.mp4" -File)
if ($clips.Count -ne 16) {
    throw "Expected 16 exported clips, found $($clips.Count)."
}

$readyPath = Join-Path $workspace "gui-ready.txt"
$gui = Join-Path $packageRoot "VidChopper.exe"
$guiProcess = Start-Process -FilePath $gui -ArgumentList @(
    "--demo-scene=workspace",
    "--demo-source=$sourceVideo",
    "--window-size=800x600",
    "--demo-ready-file=$readyPath"
) -PassThru -WindowStyle Hidden
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $readyPath) {
            if ((Get-Content -Raw -LiteralPath $readyPath).Trim() -ne "ready") {
                throw "Packaged GUI wrote an invalid ready marker."
            }
            break
        }
        if ($guiProcess.HasExited) {
            throw "Packaged GUI exited before writing its ready marker."
        }
        Start-Sleep -Milliseconds 200
    }
    if (-not (Test-Path -LiteralPath $readyPath)) {
        throw "Packaged GUI did not write its ready marker within 30 seconds."
    }
}
finally {
    if (-not $guiProcess.HasExited) {
        Stop-Process -Id $guiProcess.Id
    }
}

if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $repoRoot "artifacts\release\release-candidate-evidence.json"
}
$EvidencePath = [IO.Path]::GetFullPath($EvidencePath)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $EvidencePath) | Out-Null
$archiveInfo = Get-Item -LiteralPath $archive
$evidence = [ordered]@{
    schemaVersion = 1
    version = $Version
    archiveName = $archiveInfo.Name
    archiveBytes = $archiveInfo.Length
    archiveSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
    cliVersion = $versionOutput
    chapterBuilderPlanned = 16
    chapterBuilderExported = 16
    guiReady = $true
    packagedCliQtFree = $true
    ffmpegBundled = $false
    runnerOS = [Environment]::OSVersion.VersionString
    verifiedAtUtc = [DateTime]::UtcNow.ToString("o")
}
$evidence | ConvertTo-Json | Set-Content -LiteralPath $EvidencePath -Encoding utf8
Write-Output "Release archive smoke test passed: $EvidencePath"
