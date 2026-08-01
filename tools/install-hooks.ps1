param(
    [switch]$Remove
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$gitDirectory = (& git -C $repoRoot rev-parse --git-dir 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($gitDirectory)) {
    throw "The repository Git directory could not be resolved."
}
if (-not [System.IO.Path]::IsPathRooted($gitDirectory)) {
    $gitDirectory = Join-Path $repoRoot $gitDirectory
}

$hookPath = Join-Path $gitDirectory "hooks\pre-push"
$marker = "# Managed by VidChopper tools/install-hooks.ps1"

function Test-ManagedHook {
    param([Parameter(Mandatory = $true)][string]$Content)

    return @($Content -split "`r?`n") -contains $marker
}

if ($Remove) {
    if (-not (Test-Path -LiteralPath $hookPath)) {
        Write-Host "No VidChopper pre-push hook is installed."
        exit 0
    }

    $content = Get-Content -Raw -LiteralPath $hookPath
    if (-not (Test-ManagedHook -Content $content)) {
        throw "Refusing to remove an unmanaged pre-push hook at $hookPath."
    }

    Remove-Item -LiteralPath $hookPath
    Write-Host "Removed the VidChopper pre-push hook."
    exit 0
}

if (Test-Path -LiteralPath $hookPath) {
    $content = Get-Content -Raw -LiteralPath $hookPath
    if (-not (Test-ManagedHook -Content $content)) {
        throw "A pre-push hook already exists at $hookPath. Preserve or remove it before installing the VidChopper hook."
    }
}

$hook = @"
#!/bin/sh
$marker
repo_root=`$(git rev-parse --show-toplevel) || exit 1
pwsh -NoProfile -File "`$repo_root/tools/verify.ps1" -Tier Quick
"@

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $hookPath) | Out-Null
[System.IO.File]::WriteAllText($hookPath, $hook.Replace("`r`n", "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Installed the repository-local pre-push hook at $hookPath. No global Git configuration was changed."
