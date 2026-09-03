$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "capture-demo-common.ps1")

function Assert-Equal {
    param(
        [Parameter(Mandatory = $true)]$Actual,
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Actual -ne $Expected) {
        throw "$Label expected '$Expected', got '$Actual'."
    }
}

$crop = [pscustomobject]@{
    x = 18
    y = 72
    width = 1262
    height = 468
}

$native = Resolve-CaptureRectangle -BitmapWidth 1296 -BitmapHeight 932 -WindowSize "1294x925" -Crop $crop
Assert-Equal -Actual $native.X -Expected 18 -Label "Native X"
Assert-Equal -Actual $native.Y -Expected 72 -Label "Native Y"
Assert-Equal -Actual $native.Width -Expected 1262 -Label "Native width"
Assert-Equal -Actual $native.Height -Expected 468 -Label "Native height"

$constrained = Resolve-CaptureRectangle -BitmapWidth 1024 -BitmapHeight 768 -WindowSize "1294x925" -Crop $crop
Assert-Equal -Actual $constrained.X -Expected 14 -Label "Constrained X"
Assert-Equal -Actual $constrained.Y -Expected 60 -Label "Constrained Y"
Assert-Equal -Actual $constrained.Width -Expected 999 -Label "Constrained width"
Assert-Equal -Actual $constrained.Height -Expected 389 -Label "Constrained height"

if ($constrained.Right -gt 1024 -or $constrained.Bottom -gt 768) {
    throw "Constrained crop exceeds the captured bitmap bounds."
}

$manifest = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot "demo-capture-manifest.json") | ConvertFrom-Json
foreach ($capture in @($manifest.captures | Where-Object capture -eq "crop")) {
    $bounds = Resolve-CaptureRectangle `
        -BitmapWidth 1024 `
        -BitmapHeight 768 `
        -WindowSize $capture.windowSize `
        -Crop $capture.crop
    if ($bounds.X -lt 0 -or $bounds.Y -lt 0 -or $bounds.Right -gt 1024 -or $bounds.Bottom -gt 768) {
        throw "Scaled crop for '$($capture.asset)' exceeds the captured bitmap bounds."
    }
}

$invalidCrop = [pscustomobject]@{ x = 0; y = 0; width = 1295; height = 100 }
$rejectedInvalidCrop = $false
try {
    [void](Resolve-CaptureRectangle `
            -BitmapWidth 1296 `
            -BitmapHeight 932 `
            -WindowSize "1294x925" `
            -Crop $invalidCrop)
}
catch {
    $rejectedInvalidCrop = $_.Exception.Message -eq "Demo crop exceeds its reference window bounds."
}
if (-not $rejectedInvalidCrop) {
    throw "A crop outside its reference window was not rejected."
}

Write-Host "Demo capture crop tests passed."
