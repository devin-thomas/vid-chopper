param(
    [ValidateSet("Check", "Write")][string]$Mode = "Check",
    [string]$SourceCommit = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$skillVersion = "1.0.0"
$skillContractVersion = 1
$skillRepoRoot = ".agents/skills/vidchopper-cli"
$skillRoot = Join-Path $repoRoot ".agents\skills\vidchopper-cli"
$artifactRoot = Join-Path $repoRoot "packaging\releases\agent-skills\v$skillVersion"
$manifestPath = Join-Path $artifactRoot "manifest.json"
$archivePath = Join-Path $artifactRoot "vidchopper-cli.zip"
$versionedSkillPath = Join-Path $artifactRoot "SKILL.md"
$indexPath = Join-Path $artifactRoot "index.json"
$utf8NoBom = [Text.UTF8Encoding]::new($false, $true)

$expectedSkillFiles = @(
    "SKILL.md",
    "agents/openai.yaml",
    "assets/chapter-config.example.json",
    "assets/chapter-config.example.yaml",
    "assets/chapter-config.schema.json",
    "assets/chapterbuilder-tns-2xko-36.json",
    "references/chapterfile.md",
    "references/cli.md",
    "references/manifests.md"
)
$expectedArtifactFiles = @("SKILL.md", "index.json", "manifest.json", "vidchopper-cli.zip")

function Invoke-Git {
    param([Parameter(Mandatory = $true)][string[]]$GitArguments)

    $output = @(& git -C $repoRoot @GitArguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArguments -join ' ') failed:`n$($output -join "`n")"
    }
    return $output
}

function Get-LowerSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Get-StreamSha256 {
    param([Parameter(Mandatory = $true)][IO.Stream]$Stream)

    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        return [Convert]::ToHexString($algorithm.ComputeHash($Stream)).ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Assert-StringArraysEqual {
    param(
        [Parameter(Mandatory = $true)][string[]]$Actual,
        [Parameter(Mandatory = $true)][string[]]$Expected,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $actualJson = ConvertTo-Json @($Actual | Sort-Object) -Compress
    $expectedJson = ConvertTo-Json @($Expected | Sort-Object) -Compress
    if ($actualJson -ne $expectedJson) {
        throw "$Label drifted. Expected $expectedJson, got $actualJson."
    }
}

function Assert-PortableText {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        throw "Agent skill text must not use a UTF-8 byte-order mark: $Path"
    }
    try {
        $text = $utf8NoBom.GetString($bytes)
    }
    catch {
        throw "Agent skill text must be valid UTF-8: $Path"
    }
    if ($text.Contains("`r")) {
        throw "Agent skill text must use LF line endings: $Path"
    }
    if (-not $text.EndsWith("`n", [StringComparison]::Ordinal)) {
        throw "Agent skill text must end with a newline: $Path"
    }
    return $text
}

function Assert-CanonicalSkill {
    $tracked = @(Invoke-Git -GitArguments @("ls-files", "--", $skillRepoRoot)) |
        ForEach-Object { $_.Trim().Substring($skillRepoRoot.Length + 1).Replace("\", "/") }
    Assert-StringArraysEqual -Actual $tracked -Expected $expectedSkillFiles -Label "Tracked agent skill inventory"

    foreach ($relative in $expectedSkillFiles) {
        $path = Join-Path $skillRoot $relative.Replace("/", [IO.Path]::DirectorySeparatorChar)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Agent skill file is missing: $relative"
        }
        $item = Get-Item -LiteralPath $path -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Agent skill files must not be links or reparse points: $relative"
        }
        [void](Assert-PortableText -Path $path)
    }

    $skillText = Assert-PortableText -Path (Join-Path $skillRoot "SKILL.md")
    if (($skillText -split "`n").Count -gt 500) {
        throw "SKILL.md must stay at or below 500 lines."
    }
    if ($skillText -notmatch '(?s)\A---\n(?<frontmatter>.*?)\n---\n') {
        throw "SKILL.md frontmatter is missing or malformed."
    }
    $frontmatter = $Matches.frontmatter
    foreach ($required in @(
        '^name: vidchopper-cli$',
        '^description: .+$',
        '^license: MIT$',
        'vidchopper\.skill-contract-version: "1"',
        'vidchopper\.cli-version: "1\.0\.0"',
        'vidchopper\.chapterfile-schema-version: "1"',
        'vidchopper\.export-manifest-schema-version: "1"'
    )) {
        if ($frontmatter -notmatch "(?m)$required") {
            throw "SKILL.md frontmatter contract drifted: $required"
        }
    }
    foreach ($requiredText in @(
        'Keep the source video, ChapterFile, prompt, local paths, clips, and manifests on the user''s machine.',
        'Planned chapters: N',
        'Existing output: yes',
        'Immediately before export',
        'The `v1.0.0` application ZIP',
        'contains this skill and its adjacent manifest'
    )) {
        if (-not $skillText.Contains($requiredText, [StringComparison]::Ordinal)) {
            throw "SKILL.md safety contract drifted: $requiredText"
        }
    }
    if ($skillText -match '(?i)\bTODO\b|\[TODO') {
        throw "SKILL.md still contains placeholder text."
    }

    $linkedResources = @([regex]::Matches(
            $skillText,
            '\]\((?<path>(?:references|assets)/[^)#?]+)\)'
        ) | ForEach-Object { $_.Groups['path'].Value })
    foreach ($resource in $linkedResources) {
        $resolved = Join-Path $skillRoot $resource.Replace("/", [IO.Path]::DirectorySeparatorChar)
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "SKILL.md links to a missing resource: $resource"
        }
    }
    foreach ($reference in @("references/cli.md", "references/chapterfile.md", "references/manifests.md")) {
        if ($reference -notin $linkedResources) {
            throw "SKILL.md must directly link $reference."
        }
    }

    $openAiYaml = Assert-PortableText -Path (Join-Path $skillRoot "agents\openai.yaml")
    if (-not $openAiYaml.Contains('display_name: "VidChopper CLI"', [StringComparison]::Ordinal) -or
        -not $openAiYaml.Contains('Use $vidchopper-cli', [StringComparison]::Ordinal)) {
        throw "agents/openai.yaml no longer matches the skill interface."
    }

    $cliSource = [IO.File]::ReadAllText((Join-Path $repoRoot "src\cli\cli_arguments.cpp"))
    $releasedFlags = @([regex]::Matches($cliSource, '"(?<flag>--[a-z0-9-]+)"') |
        ForEach-Object { $_.Groups['flag'].Value } | Sort-Object -Unique)
    $cliReference = Assert-PortableText -Path (Join-Path $skillRoot "references\cli.md")
    $documentedFlags = @([regex]::Matches($cliReference, '(?m)^\| `(?<flag>--[a-z0-9-]+)` \|') |
        ForEach-Object { $_.Groups['flag'].Value } | Sort-Object -Unique)
    Assert-StringArraysEqual -Actual $documentedFlags -Expected $releasedFlags -Label "Documented CLI flags"

    foreach ($form in @(
        '& "C:\Tools\VidChopper\VidChopperCLI.exe" `',
        '& "C:\Tools\VidChopper\VidChopperCLI.exe" chop `',
        '  --embedded `',
        '--dry-run'
    )) {
        if (-not $skillText.Contains($form, [StringComparison]::Ordinal)) {
            throw "SKILL.md is missing a released command example: $form"
        }
    }

    $assetPairs = [ordered]@{
        "assets/chapter-config.schema.json" = "docs/schemas/chapter-config/v1/schema.json"
        "assets/chapter-config.example.json" = "examples/chapter-config/v1/chapter-config.json"
        "assets/chapter-config.example.yaml" = "examples/chapter-config/v1/chapter-config.yaml"
        "assets/chapterbuilder-tns-2xko-36.json" = "tests/fixtures/chapterbuilder/tns-2xko-36-chapters.json"
    }
    foreach ($pair in $assetPairs.GetEnumerator()) {
        $skillAssetPath = "$skillRepoRoot/$($pair.Key)"
        $skillBlob = (@(Invoke-Git -GitArguments @("rev-parse", "HEAD:$skillAssetPath")))[-1].Trim()
        $canonicalBlob = (@(Invoke-Git -GitArguments @("rev-parse", "HEAD:$($pair.Value)")))[-1].Trim()
        if ($skillBlob -ne $canonicalBlob) {
            throw "Bundled skill asset drifted from $($pair.Value): $($pair.Key)"
        }
    }

    $schema = Get-Content -Raw -LiteralPath (Join-Path $skillRoot "assets\chapter-config.schema.json") |
        ConvertFrom-Json
    if ($schema.properties.version.const -ne 1) {
        throw "Bundled ChapterFile schema version must be 1."
    }

    $directorySkills = @(Invoke-Git -GitArguments @("ls-files", "--", ".agents/skills/*/SKILL.md")) |
        ForEach-Object { $_.Trim().Replace("\", "/") }
    Assert-StringArraysEqual -Actual $directorySkills -Expected @("$skillRepoRoot/SKILL.md") `
        -Label "Project-scanning harness discovery"

    return @($releasedFlags)
}

function Resolve-SourceCommit {
    param([Parameter(Mandatory = $true)][string]$Commit)

    if ($Commit -notmatch '^[a-f0-9]{40}$') {
        throw "SourceCommit must be a full lowercase Git commit SHA."
    }
    $resolved = (@(Invoke-Git -GitArguments @("rev-parse", "$Commit^{commit}")))[-1].Trim()
    if ($resolved -ne $Commit) {
        throw "SourceCommit did not resolve exactly: $Commit"
    }
    & git -C $repoRoot merge-base --is-ancestor $Commit HEAD
    if ($LASTEXITCODE -ne 0) {
        throw "SourceCommit must remain an ancestor of HEAD: $Commit"
    }

    foreach ($relative in $expectedSkillFiles) {
        $repoPath = "$skillRepoRoot/$relative"
        $sourceBlob = (@(Invoke-Git -GitArguments @("rev-parse", "${Commit}:$repoPath")))[-1].Trim()
        $headBlob = (@(Invoke-Git -GitArguments @("rev-parse", "HEAD:$repoPath")))[-1].Trim()
        if ($sourceBlob -ne $headBlob) {
            throw "SourceCommit does not identify the canonical $repoPath bytes."
        }
        $workingBlob = (@(Invoke-Git -GitArguments @("hash-object", "--path=$repoPath", "--", $repoPath)))[-1].Trim()
        if ($workingBlob -ne $headBlob) {
            throw "Canonical skill has uncommitted byte drift: $repoPath"
        }
    }
    return $Commit
}

function Write-Utf8Lf {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text
    )

    $normalized = $Text.Replace("`r`n", "`n").Replace("`r", "`n")
    if (-not $normalized.EndsWith("`n", [StringComparison]::Ordinal)) {
        $normalized += "`n"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path, $normalized, $utf8NoBom)
}

function Set-DeterministicZipPlatform {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    $minimumEocdLength = 22
    $maximumCommentLength = 65535
    $minimumOffset = [Math]::Max(0, $bytes.Length - $minimumEocdLength - $maximumCommentLength)
    $eocdOffset = -1
    for ($index = $bytes.Length - $minimumEocdLength; $index -ge $minimumOffset; --$index) {
        if ($bytes[$index] -eq 0x50 -and $bytes[$index + 1] -eq 0x4B -and
            $bytes[$index + 2] -eq 0x05 -and $bytes[$index + 3] -eq 0x06) {
            $eocdOffset = $index
            break
        }
    }
    if ($eocdOffset -lt 0) {
        throw "Generated agent skill ZIP has no end-of-central-directory record."
    }

    $entryCount = [BitConverter]::ToUInt16($bytes, $eocdOffset + 10)
    $centralOffset = [int][BitConverter]::ToUInt32($bytes, $eocdOffset + 16)
    for ($entryIndex = 0; $entryIndex -lt $entryCount; ++$entryIndex) {
        if ($centralOffset + 46 -gt $eocdOffset -or
            $bytes[$centralOffset] -ne 0x50 -or $bytes[$centralOffset + 1] -ne 0x4B -or
            $bytes[$centralOffset + 2] -ne 0x01 -or $bytes[$centralOffset + 3] -ne 0x02) {
            throw "Generated agent skill ZIP has a malformed central directory."
        }
        $bytes[$centralOffset + 5] = 3 # Pin the ZIP creator platform to Unix on every runner.
        $nameLength = [BitConverter]::ToUInt16($bytes, $centralOffset + 28)
        $extraLength = [BitConverter]::ToUInt16($bytes, $centralOffset + 30)
        $commentLength = [BitConverter]::ToUInt16($bytes, $centralOffset + 32)
        $centralOffset += 46 + $nameLength + $extraLength + $commentLength
    }
    if ($centralOffset -ne $eocdOffset) {
        throw "Generated agent skill ZIP central-directory size drifted."
    }
    [IO.File]::WriteAllBytes($Path, $bytes)
}

function New-DeterministicSkillArchive {
    param([Parameter(Mandatory = $true)][string]$Destination)

    $fullDestination = [IO.Path]::GetFullPath($Destination)
    $fullParent = [IO.Path]::GetFullPath((Split-Path -Parent $Destination))
    if ([IO.Path]::GetFullPath((Split-Path -Parent $fullDestination)) -ne $fullParent -or
        [IO.Path]::GetFileName($fullDestination) -ne "vidchopper-cli.zip") {
        throw "Refusing to replace an unexpected agent skill archive: $fullDestination"
    }
    New-Item -ItemType Directory -Force -Path $fullParent | Out-Null
    if (Test-Path -LiteralPath $fullDestination) {
        Remove-Item -LiteralPath $fullDestination -Force
    }

    Add-Type -AssemblyName System.IO.Compression
    $stream = [IO.File]::Open($fullDestination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try {
        $archive = [IO.Compression.ZipArchive]::new(
            $stream,
            [IO.Compression.ZipArchiveMode]::Create,
            $false,
            [Text.Encoding]::UTF8
        )
        try {
            foreach ($relative in @($expectedSkillFiles | Sort-Object)) {
                $entryName = $relative
                $entry = $archive.CreateEntry($entryName, [IO.Compression.CompressionLevel]::NoCompression)
                $entry.LastWriteTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                $entry.ExternalAttributes = -2119958528 # regular file, mode 0644
                $input = [IO.File]::OpenRead((Join-Path $skillRoot $relative.Replace("/", [IO.Path]::DirectorySeparatorChar)))
                try {
                    $output = $entry.Open()
                    try {
                        $input.CopyTo($output)
                    }
                    finally {
                        $output.Dispose()
                    }
                }
                finally {
                    $input.Dispose()
                }
            }
        }
        finally {
            $archive.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
    Set-DeterministicZipPlatform -Path $fullDestination
}

function Write-GeneratedArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][string]$Commit,
        [Parameter(Mandatory = $true)][string[]]$CliFlags
    )

    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    $generatedArchive = Join-Path $OutputRoot "vidchopper-cli.zip"
    New-DeterministicSkillArchive -Destination $generatedArchive
    Copy-Item -LiteralPath (Join-Path $skillRoot "SKILL.md") `
        -Destination (Join-Path $OutputRoot "SKILL.md") -Force

    $files = @($expectedSkillFiles | Sort-Object | ForEach-Object {
            $filePath = Join-Path $skillRoot $_.Replace("/", [IO.Path]::DirectorySeparatorChar)
            [ordered]@{ path = $_; sha256 = Get-LowerSha256 -Path $filePath }
        })
    $manifest = [ordered]@{
        skillContractVersion = $skillContractVersion
        cliVersion = $skillVersion
        chapterFileSchemaVersion = 1
        exportManifestSchemaVersion = 1
        skillSha256 = Get-LowerSha256 -Path (Join-Path $skillRoot "SKILL.md")
        skillArchiveSha256 = Get-LowerSha256 -Path $generatedArchive
        chapterFileSchemaSha256 = Get-LowerSha256 -Path (Join-Path $skillRoot "assets\chapter-config.schema.json")
        sourceCommit = $Commit
        repositoryPath = "$skillRepoRoot/SKILL.md"
        stableUrl = "https://vidchopper.app/agents/vidchopper-cli/SKILL.md"
        versionedUrl = "https://vidchopper.app/agents/vidchopper-cli/v$skillVersion/SKILL.md"
        archiveUrl = "https://vidchopper.app/agents/vidchopper-cli/v$skillVersion/vidchopper-cli.zip"
        cliFlags = @($CliFlags | Sort-Object)
        files = $files
    }
    Write-Utf8Lf -Path (Join-Path $OutputRoot "manifest.json") `
        -Text ($manifest | ConvertTo-Json -Depth 10)

    $index = [ordered]@{
        '$schema' = "https://schemas.agentskills.io/discovery/0.2.0/schema.json"
        skills = @([ordered]@{
                name = "vidchopper-cli"
                type = "archive"
                description = "Plan and export local chapter clips safely with VidChopperCLI."
                url = "/agents/vidchopper-cli/v$skillVersion/vidchopper-cli.zip"
                digest = "sha256:$($manifest.skillArchiveSha256)"
            })
    }
    Write-Utf8Lf -Path (Join-Path $OutputRoot "index.json") `
        -Text ($index | ConvertTo-Json -Depth 10)
}

function Assert-GeneratedArtifacts {
    param([Parameter(Mandatory = $true)][string]$Commit)

    foreach ($relative in $expectedArtifactFiles) {
        $path = Join-Path $artifactRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Generated agent skill artifact is missing: packaging/releases/agent-skills/v$skillVersion/$relative"
        }
    }
    $trackedArtifacts = @(Invoke-Git -GitArguments @(
            "ls-files", "--", "packaging/releases/agent-skills/v$skillVersion"
        )) | ForEach-Object { [IO.Path]::GetFileName($_.Trim()) }
    Assert-StringArraysEqual -Actual $trackedArtifacts -Expected $expectedArtifactFiles `
        -Label "Tracked generated agent skill artifacts"

    $checkRoot = Join-Path $repoRoot "artifacts\agent-skill-check"
    $temporaryRoot = Join-Path $checkRoot ([Guid]::NewGuid().ToString("N"))
    $fullCheckRoot = [IO.Path]::GetFullPath($checkRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $fullTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
    if (-not $fullTemporaryRoot.StartsWith("$fullCheckRoot$([IO.Path]::DirectorySeparatorChar)",
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Agent skill comparison directory escaped its bounded root."
    }
    try {
        Write-GeneratedArtifacts -OutputRoot $temporaryRoot -Commit $Commit -CliFlags $releasedFlags
        foreach ($relative in $expectedArtifactFiles) {
            $expected = Get-LowerSha256 -Path (Join-Path $temporaryRoot $relative)
            $actual = Get-LowerSha256 -Path (Join-Path $artifactRoot $relative)
            if ($actual -ne $expected) {
                throw "Generated agent skill artifact drifted: $relative"
            }
        }
    }
    finally {
        if (Test-Path -LiteralPath $temporaryRoot) {
            Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
        }
    }

    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    if ($manifest.skillContractVersion -ne 1 -or $manifest.cliVersion -ne $skillVersion -or
        $manifest.chapterFileSchemaVersion -ne 1 -or $manifest.exportManifestSchemaVersion -ne 1 -or
        $manifest.sourceCommit -ne $Commit) {
        throw "Agent skill manifest version tuple drifted."
    }
    if ($manifest.skillSha256 -ne (Get-LowerSha256 -Path (Join-Path $skillRoot "SKILL.md")) -or
        $manifest.skillArchiveSha256 -ne (Get-LowerSha256 -Path $archivePath) -or
        $manifest.chapterFileSchemaSha256 -ne (Get-LowerSha256 -Path (Join-Path $skillRoot "assets\chapter-config.schema.json"))) {
        throw "Agent skill manifest digest drifted."
    }
    Assert-StringArraysEqual -Actual @($manifest.cliFlags) -Expected $releasedFlags `
        -Label "Agent skill manifest CLI flags"

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $expectedEntries = @($expectedSkillFiles | Sort-Object)
        $actualEntries = @($zip.Entries | ForEach-Object { $_.FullName })
        Assert-StringArraysEqual -Actual $actualEntries -Expected $expectedEntries -Label "Agent skill ZIP inventory"
        $manifestFiles = @{}
        foreach ($file in $manifest.files) {
            $manifestFiles[[string]$file.path] = [string]$file.sha256
        }
        foreach ($entry in $zip.Entries) {
            $relative = $entry.FullName
            $entryStream = $entry.Open()
            try {
                $entryHash = Get-StreamSha256 -Stream $entryStream
            }
            finally {
                $entryStream.Dispose()
            }
            if ($manifestFiles[$relative] -ne $entryHash) {
                throw "Agent skill ZIP entry digest drifted: $relative"
            }
        }
    }
    finally {
        $zip.Dispose()
    }

    $index = Get-Content -Raw -LiteralPath $indexPath | ConvertFrom-Json
    $entry = $index.skills[0]
    if ($index.'$schema' -ne "https://schemas.agentskills.io/discovery/0.2.0/schema.json" -or
        $index.skills.Count -ne 1 -or $entry.name -ne "vidchopper-cli" -or
        $entry.type -ne "archive" -or
        $entry.url -ne "/agents/vidchopper-cli/v$skillVersion/vidchopper-cli.zip" -or
        $entry.digest -ne "sha256:$($manifest.skillArchiveSha256)") {
        throw "Agent skill discovery index drifted."
    }

    $routes = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "docs\routes.json") | ConvertFrom-Json
    $expectedRoutes = [ordered]@{
        "/agents/vidchopper-cli/SKILL.md" = @("$skillRepoRoot/SKILL.md", "stable")
        "/agents/vidchopper-cli/manifest.json" = @("packaging/releases/agent-skills/v$skillVersion/manifest.json", "stable")
        "/agents/vidchopper-cli/v$skillVersion/SKILL.md" = @("packaging/releases/agent-skills/v$skillVersion/SKILL.md", "immutable")
        "/agents/vidchopper-cli/v$skillVersion/manifest.json" = @("packaging/releases/agent-skills/v$skillVersion/manifest.json", "immutable")
        "/agents/vidchopper-cli/v$skillVersion/vidchopper-cli.zip" = @("packaging/releases/agent-skills/v$skillVersion/vidchopper-cli.zip", "immutable")
        "/.well-known/agent-skills/index.json" = @("packaging/releases/agent-skills/v$skillVersion/index.json", "stable")
    }
    foreach ($route in $expectedRoutes.GetEnumerator()) {
        $asset = @($routes.assets | Where-Object { $_.route -eq $route.Key })
        if ($asset.Count -ne 1 -or $asset[0].source -ne $route.Value[0] -or
            $asset[0].cache -ne $route.Value[1]) {
            throw "Generic URL harness route drifted: $($route.Key)"
        }
        if ($asset[0].cache -eq "immutable") {
            $source = Join-Path $repoRoot $asset[0].source.Replace("/", [IO.Path]::DirectorySeparatorChar)
            if ($asset[0].sha256 -ne (Get-LowerSha256 -Path $source)) {
                throw "Immutable route digest drifted: $($route.Key)"
            }
        }
    }
    if (@($routes.deferredAssets | Where-Object { $_.owner -eq "VID-52" }).Count -ne 0) {
        throw "VID-52 routes must no longer be deferred."
    }
}

$releasedFlags = @(Assert-CanonicalSkill)

if ($Mode -eq "Write") {
    if ([string]::IsNullOrWhiteSpace($SourceCommit)) {
        throw "-SourceCommit is required in Write mode."
    }
    $resolvedCommit = Resolve-SourceCommit -Commit $SourceCommit
    Write-GeneratedArtifacts -OutputRoot $artifactRoot -Commit $resolvedCommit -CliFlags $releasedFlags
    Write-Output "Generated deterministic agent skill artifacts for $resolvedCommit."
    exit 0
}

if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Agent skill manifest is missing; run Write mode first."
}
$storedManifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$resolvedCommit = Resolve-SourceCommit -Commit ([string]$storedManifest.sourceCommit)
Assert-GeneratedArtifacts -Commit $resolvedCommit
Write-Output "Agent skill contract and deterministic artifacts are valid."
