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
$patches = @(
    @('$repoRoot = Split-Path -Parent $PSScriptRoot', '$repoRoot = $env:VIDCHOPPER_AGENT_SKILL_REPO_ROOT'),
    @('$skillVersion = "1.0.0"', '$skillVersion = "1.2.0"'),
    @('vidchopper\.cli-version: "1\.0\.0"', 'vidchopper\.cli-version: "1\.2\.0"'),
    @('The `v1.0.0` application ZIP', 'The `v1.2.0` agent-skill package')
)

foreach ($patch in $patches) {
    $from = [string]$patch[0]
    $to = [string]$patch[1]
    if (-not $core.Contains($from, [StringComparison]::Ordinal)) {
        throw "Agent skill artifact core no longer contains the expected 1.0.0 contract text: $from"
    }
    $core = $core.Replace($from, $to, [StringComparison]::Ordinal)
}

$environmentName = "VIDCHOPPER_AGENT_SKILL_REPO_ROOT"
$previousRoot = [Environment]::GetEnvironmentVariable($environmentName, "Process")
try {
    [Environment]::SetEnvironmentVariable(
        $environmentName,
        (Split-Path -Parent $PSScriptRoot),
        "Process"
    )
    $runner = [ScriptBlock]::Create($core)
    & $runner -Mode $Mode -SourceCommit $SourceCommit
}
finally {
    [Environment]::SetEnvironmentVariable($environmentName, $previousRoot, "Process")
}
