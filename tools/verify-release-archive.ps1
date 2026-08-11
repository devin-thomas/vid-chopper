param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$ArchivePath,
    [string]$EvidencePath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
# The packaged CLI uses the stable agent skill contract even when the application
# candidate has advanced to a newer release version.
$skillVersion = "1.0.0"
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
    "LICENSE",
    ".agents\skills\vidchopper-cli\SKILL.md",
    ".agents\skills\vidchopper-cli.manifest.json"
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

& (Join-Path $repoRoot "tools\agent-skill-artifacts.ps1") -Mode Check
if ($LASTEXITCODE -ne 0) {
    throw "Repository agent skill validation failed."
}
$packagedSkillRoot = Join-Path $packageRoot ".agents\skills\vidchopper-cli"
$packagedSkillManifestPath = Join-Path $packageRoot ".agents\skills\vidchopper-cli.manifest.json"
$canonicalSkillManifestPath = Join-Path $repoRoot "packaging\releases\agent-skills\v$skillVersion\manifest.json"
$packagedManifestHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $packagedSkillManifestPath).Hash
$canonicalManifestHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $canonicalSkillManifestPath).Hash
if ($packagedManifestHash -ne $canonicalManifestHash) {
    throw "Packaged agent skill manifest does not match the canonical release manifest."
}
$skillManifest = Get-Content -Raw -LiteralPath $packagedSkillManifestPath | ConvertFrom-Json
if ($skillManifest.skillContractVersion -ne 1 -or $skillManifest.cliVersion -ne $skillVersion -or
    $skillManifest.chapterFileSchemaVersion -ne 1 -or $skillManifest.exportManifestSchemaVersion -ne 1) {
    throw "Packaged agent skill compatibility tuple does not match skill version $skillVersion."
}
$expectedSkillFiles = @($skillManifest.files.path | Sort-Object)
$packagedSkillFiles = @(Get-ChildItem -LiteralPath $packagedSkillRoot -Recurse -File | ForEach-Object {
        [IO.Path]::GetRelativePath($packagedSkillRoot, $_.FullName).Replace("\", "/")
    } | Sort-Object)
if ((ConvertTo-Json $expectedSkillFiles -Compress) -ne (ConvertTo-Json $packagedSkillFiles -Compress)) {
    throw "Packaged agent skill inventory does not match its manifest."
}
foreach ($file in $skillManifest.files) {
    $relative = [string]$file.path
    if ($relative.StartsWith("/", [StringComparison]::Ordinal) -or $relative.Contains("\") -or
        $relative.Split("/").Contains("..")) {
        throw "Packaged agent skill manifest contains an unsafe path: $relative"
    }
    $packagedPath = Join-Path $packagedSkillRoot $relative.Replace("/", [IO.Path]::DirectorySeparatorChar)
    $canonicalPath = Join-Path $repoRoot ".agents\skills\vidchopper-cli\$($relative.Replace('/', '\'))"
    $packagedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $packagedPath).Hash.ToLowerInvariant()
    $canonicalHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $canonicalPath).Hash.ToLowerInvariant()
    if ($packagedHash -ne $file.sha256 -or $canonicalHash -ne $file.sha256) {
        throw "Packaged agent skill file digest drifted: $relative"
    }
}
$packagedSkillPath = Join-Path $packagedSkillRoot "SKILL.md"
$packagedSchemaPath = Join-Path $packagedSkillRoot "assets\chapter-config.schema.json"
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $packagedSkillPath).Hash.ToLowerInvariant() -ne
        $skillManifest.skillSha256 -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $packagedSchemaPath).Hash.ToLowerInvariant() -ne
        $skillManifest.chapterFileSchemaSha256) {
    throw "Packaged agent skill entry or schema digest drifted."
}

$packagedHelpFlags = @([regex]::Matches($helpOutput, '--[a-z0-9-]+') |
    ForEach-Object { $_.Value } | Sort-Object -Unique)
$manifestFlags = @($skillManifest.cliFlags | Sort-Object)
if ((ConvertTo-Json $packagedHelpFlags -Compress) -ne (ConvertTo-Json $manifestFlags -Compress)) {
    throw "Packaged CLI help flags drifted from the agent skill manifest."
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

$smallJson = Join-Path $packagedSkillRoot "assets\chapter-config.example.json"
$smallJsonOutput = (& $cli $sourceVideo $smallJson --dry-run 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $smallJsonOutput -notmatch "Planned chapters: 3") {
    throw "Packaged CLI did not plan the bundled JSON skill example."
}
$smallYaml = Join-Path $packagedSkillRoot "assets\chapter-config.example.yaml"
$smallYamlOutput = (& $cli chop $sourceVideo $smallYaml --dry-run 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $smallYamlOutput -notmatch "Planned chapters: 3") {
    throw "Packaged CLI did not plan the bundled YAML skill example."
}

$fixture = Join-Path $packagedSkillRoot "assets\chapterbuilder-tns-2xko-36.json"
$dryRunOutput = (& $cli $sourceVideo $fixture --dry-run 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $dryRunOutput -notmatch "Planned chapters: 16") {
    throw "Packaged CLI did not plan all 16 ChapterBuilder chapters."
}
$chopOutput = (& $cli chop $sourceVideo $fixture --dry-run 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $chopOutput -notmatch "Planned chapters: 16") {
    throw "Packaged CLI chop mode did not plan all 16 ChapterBuilder chapters."
}

$outputDirectory = Join-Path $mediaRoot "tns-2xko-36-synthetic_chapters"
if ((Test-Path -LiteralPath (Join-Path $isolatedCliRoot "VidChopperCLI.ini")) -or
    (Test-Path -LiteralPath $outputDirectory)) {
    throw "A packaged CLI dry-run created settings or output artifacts."
}

$exportOutput = (& $cli $sourceVideo $fixture --preset ultrafast --crf 51 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $exportOutput -notmatch "Summary: exported=16") {
    throw "Packaged CLI did not export all 16 ChapterBuilder chapters."
}

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
foreach ($segment in $manifest.segments) {
    $probeOutput = (& ffprobe -v error -show_entries format=duration `
        -of default=noprint_wrappers=1:nokey=1 $segment.outputPath 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "ffprobe could not verify packaged output: $($segment.outputPath)"
    }
    $actualSeconds = [double]::Parse($probeOutput.Trim(), [Globalization.CultureInfo]::InvariantCulture)
    $plannedSeconds = [double]$segment.durationMs / 1000.0
    if ([Math]::Abs($actualSeconds - $plannedSeconds) -gt 1.0) {
        throw "Packaged output duration exceeded one-second tolerance: $($segment.outputPath)"
    }
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
$inventoryText = (($skillManifest.files | ForEach-Object { "$($_.path) $($_.sha256)" }) -join "`n") + "`n"
$inventoryBytes = [Text.Encoding]::UTF8.GetBytes($inventoryText)
$inventoryHasher = [Security.Cryptography.SHA256]::Create()
try {
    $inventorySha256 = [Convert]::ToHexString($inventoryHasher.ComputeHash($inventoryBytes)).ToLowerInvariant()
}
finally {
    $inventoryHasher.Dispose()
}
$evidence = [ordered]@{
    schemaVersion = 1
    version = $Version
    archiveName = $archiveInfo.Name
    archiveBytes = $archiveInfo.Length
    archiveSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
    cliVersion = $versionOutput
    agentSkillBundled = $true
    agentSkillSourceCommit = $skillManifest.sourceCommit
    agentSkillContractVersion = $skillManifest.skillContractVersion
    agentSkillSha256 = $skillManifest.skillSha256
    agentSkillArchiveSha256 = $skillManifest.skillArchiveSha256
    agentSkillManifestSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $packagedSkillManifestPath).Hash.ToLowerInvariant()
    agentSkillInventorySha256 = $inventorySha256
    bundledExamplesPlanned = 2
    chapterBuilderPlanned = 16
    chapterBuilderExported = 16
    chapterDurationsVerified = 16
    guiReady = $true
    packagedCliQtFree = $true
    ffmpegBundled = $false
    runnerOS = [Environment]::OSVersion.VersionString
    verifiedAtUtc = [DateTime]::UtcNow.ToString("o")
}
$evidence | ConvertTo-Json | Set-Content -LiteralPath $EvidencePath -Encoding utf8
Write-Output "Release archive smoke test passed: $EvidencePath"
