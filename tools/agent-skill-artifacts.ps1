param(
    [ValidateSet("Check", "Write")][string]$Mode = "Check",
    [string]$SourceCommit = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$corePath = Join-Path $PSScriptRoot "agent-skill-artifacts-core.ps1"
if (-not (Test-Path -LiteralPath $corePath -PathType Leaf)) {
    throw "Agent skill artifact core is missing: $corePath"
}

$core = [IO.File]::ReadAllText($corePath)

$repoRootMarker = '$repoRoot = Split-Path -Parent $PSScriptRoot'
if (-not $core.Contains($repoRootMarker)) {
    throw "Agent skill artifact core no longer contains the expected repo-root contract."
}
$core = $core.Replace($repoRootMarker, '$repoRoot = $env:VIDCHOPPER_AGENT_SKILL_REPO_ROOT')

$versionMarker = '$skillVersion = "1.0.0"'
if (-not $core.Contains($versionMarker)) {
    throw "Agent skill artifact core no longer contains the expected 1.0.0 version contract."
}
$core = $core.Replace($versionMarker, '$skillVersion = "1.2.0"')

$frontmatterMarker = 'vidchopper\.cli-version: "1\.0\.0"'
if (-not $core.Contains($frontmatterMarker)) {
    throw "Agent skill artifact core no longer contains the expected frontmatter version contract."
}
$core = $core.Replace($frontmatterMarker, 'vidchopper\.cli-version: "1\.2\.0"')

$packageMarker = 'The `v1.0.0` application ZIP'
if (-not $core.Contains($packageMarker)) {
    throw "Agent skill artifact core no longer contains the expected package-version contract."
}
$core = $core.Replace($packageMarker, 'The `v1.2.0` agent-skill package contains this')

$adjacentManifestMarker = 'contains this skill and its adjacent manifest'
if (-not $core.Contains($adjacentManifestMarker)) {
    throw "Agent skill artifact core no longer contains the expected adjacent-manifest contract."
}
$core = $core.Replace($adjacentManifestMarker, 'skill and its adjacent manifest')

$environmentName = "VIDCHOPPER_AGENT_SKILL_REPO_ROOT"
$previousRoot = [Environment]::GetEnvironmentVariable($environmentName, [EnvironmentVariableTarget]::Process)
try {
    [Environment]::SetEnvironmentVariable(
        $environmentName,
        (Split-Path -Parent $PSScriptRoot),
        [EnvironmentVariableTarget]::Process
    )
    & ([ScriptBlock]::Create($core)) -Mode $Mode -SourceCommit $SourceCommit
}
finally {
    [Environment]::SetEnvironmentVariable(
        $environmentName,
        $previousRoot,
        [EnvironmentVariableTarget]::Process
    )
}
