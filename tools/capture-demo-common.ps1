function Resolve-CaptureRectangle {
    param(
        [Parameter(Mandatory = $true)][int]$BitmapWidth,
        [Parameter(Mandatory = $true)][int]$BitmapHeight,
        [Parameter(Mandatory = $true)][string]$WindowSize,
        [Parameter(Mandatory = $true)]$Crop
    )

    if ($BitmapWidth -le 0 -or $BitmapHeight -le 0) {
        throw "Captured bitmap dimensions must be positive."
    }
    if ($WindowSize -notmatch '^(\d+)x(\d+)$') {
        throw "Invalid demo window size '$WindowSize'."
    }

    $referenceWidth = [int]$Matches[1]
    $referenceHeight = [int]$Matches[2]
    $x = [int]$Crop.x
    $y = [int]$Crop.y
    $width = [int]$Crop.width
    $height = [int]$Crop.height
    if ($referenceWidth -le 0 -or $referenceHeight -le 0 -or
        $x -lt 0 -or $y -lt 0 -or $width -le 0 -or $height -le 0) {
        throw "Demo crop and reference window dimensions must be valid and positive."
    }
    if ($x + $width -gt $referenceWidth -or $y + $height -gt $referenceHeight) {
        throw "Demo crop exceeds its reference window bounds."
    }

    if ($x + $width -gt $BitmapWidth -or $y + $height -gt $BitmapHeight) {
        $scaleX = $BitmapWidth / $referenceWidth
        $scaleY = $BitmapHeight / $referenceHeight
        $x = [int][Math]::Round($x * $scaleX, [MidpointRounding]::AwayFromZero)
        $y = [int][Math]::Round($y * $scaleY, [MidpointRounding]::AwayFromZero)
        $width = [int][Math]::Round($width * $scaleX, [MidpointRounding]::AwayFromZero)
        $height = [int][Math]::Round($height * $scaleY, [MidpointRounding]::AwayFromZero)
    }

    $x = [Math]::Min([Math]::Max(0, $x), $BitmapWidth - 1)
    $y = [Math]::Min([Math]::Max(0, $y), $BitmapHeight - 1)
    $width = [Math]::Min([Math]::Max(1, $width), $BitmapWidth - $x)
    $height = [Math]::Min([Math]::Max(1, $height), $BitmapHeight - $y)

    return [pscustomobject]@{
        X = $x
        Y = $y
        Width = $width
        Height = $height
        Right = $x + $width
        Bottom = $y + $height
    }
}
