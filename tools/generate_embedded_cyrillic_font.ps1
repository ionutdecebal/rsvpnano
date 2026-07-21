param(
    [int]$PointSize = 52,
    [string]$OutputPath = "src/display/EmbeddedCyrillicSerifFont.h",
    [string]$SymbolPrefix = "EmbeddedCyrillicSerif"
)

Add-Type -AssemblyName System.Drawing

$codepoints = @(
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0401, 0x0416, 0x0417, 0x0418,
    0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422,
    0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C,
    0x042D, 0x042E, 0x042F, 0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0451,
    0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449,
    0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F
)

$canvasWidth = 112
$canvasHeight = 128
$originX = 10
$baselineY = 76
$alphaThreshold = 16
$topPadding = 4
$bottomPadding = 2

$font = New-Object System.Drawing.Font("Times New Roman", $PointSize)
$images = @()
$globalTop = $canvasHeight
$globalBottom = -1

foreach ($cp in $codepoints) {
    $bmp = New-Object System.Drawing.Bitmap $canvasWidth, $canvasHeight
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::White)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $ch = [char]$cp
    $g.DrawString($ch, $font, [System.Drawing.Brushes]::Black, $originX, ($baselineY - $PointSize))
    $g.Dispose()

    for ($y = 0; $y -lt $canvasHeight; $y++) {
        for ($x = 0; $x -lt $canvasWidth; $x++) {
            $pixel = $bmp.GetPixel($x, $y)
            $alpha = 255 - [int]$pixel.R
            if ($alpha -gt $alphaThreshold) {
                if ($y -lt $globalTop) { $globalTop = $y }
                if ($y -gt $globalBottom) { $globalBottom = $y }
            }
        }
    }
    $images += ,$bmp
}

$cropTop = [Math]::Max(0, $globalTop - $topPadding)
$cropBottom = [Math]::Min($canvasHeight - 1, $globalBottom + $bottomPadding)
$fontHeight = $cropBottom - $cropTop + 1

$bitmapBytes = New-Object System.Collections.Generic.List[int]
$glyphEntries = New-Object System.Collections.Generic.List[string]

for ($index = 0; $index -lt $images.Count; $index++) {
    $bmp = $images[$index]
    $minX = $canvasWidth
    $maxX = -1

    for ($y = $cropTop; $y -le $cropBottom; $y++) {
        for ($x = 0; $x -lt $canvasWidth; $x++) {
            $pixel = $bmp.GetPixel($x, $y)
            $alpha = 255 - [int]$pixel.R
            if ($alpha -gt $alphaThreshold) {
                if ($x -lt $minX) { $minX = $x }
                if ($x -gt $maxX) { $maxX = $x }
            }
        }
    }

    $bitmapOffset = $bitmapBytes.Count
    if ($maxX -ge $minX) {
        $glyphWidth = $maxX - $minX + 1
        for ($y = $cropTop; $y -le $cropBottom; $y++) {
            for ($x = $minX; $x -le $maxX; $x++) {
                $pixel = $bmp.GetPixel($x, $y)
                $alpha = 255 - [int]$pixel.R
                if ($alpha -le $alphaThreshold) { $alpha = 0 }
                [void]$bitmapBytes.Add($alpha)
            }
        }
        $xOffset = $minX - $originX
        $xAdvance = [Math]::Max(8, $maxX + 4)
    } else {
        $glyphWidth = 0
        $xOffset = 0
        $xAdvance = 8
    }

    $cp = $codepoints[$index]
    $glyphEntries.Add("    {$bitmapOffset, $xOffset, $glyphWidth, $xAdvance}, // U+$("{0:X4}" -f $cp)")
    $bmp.Dispose()
}

$font.Dispose()

$lines = New-Object System.Collections.Generic.List[string]
[void]$lines.Add("#pragma once")
[void]$lines.Add("")
[void]$lines.Add("#include <Arduino.h>")
[void]$lines.Add("")
[void]$lines.Add("// Generated Cyrillic serif glyphs for Russian book rendering.")
[void]$lines.Add("// Source point size: $PointSize pt")
[void]$lines.Add("")
[void]$lines.Add("struct ${SymbolPrefix}Glyph {")
[void]$lines.Add("  uint32_t bitmapOffset;")
[void]$lines.Add("  int8_t xOffset;")
[void]$lines.Add("  uint8_t width;")
[void]$lines.Add("  uint8_t xAdvance;")
[void]$lines.Add("};")
[void]$lines.Add("")
[void]$lines.Add("constexpr uint8_t k${SymbolPrefix}Count = $($codepoints.Count);")
[void]$lines.Add("constexpr uint8_t k${SymbolPrefix}Height = $fontHeight;")
[void]$lines.Add("")
[void]$lines.Add("static const uint8_t k${SymbolPrefix}Bitmaps[] PROGMEM = {")

for ($offset = 0; $offset -lt $bitmapBytes.Count; $offset += 16) {
    $chunk = $bitmapBytes.GetRange($offset, [Math]::Min(16, $bitmapBytes.Count - $offset))
    $formatted = ($chunk | ForEach-Object { "{0,3}" -f $_ }) -join ", "
    [void]$lines.Add("    $formatted,")
}

[void]$lines.Add("};")
[void]$lines.Add("")
[void]$lines.Add("static const ${SymbolPrefix}Glyph k${SymbolPrefix}Glyphs[] PROGMEM = {")
foreach ($entry in $glyphEntries) { [void]$lines.Add($entry) }
[void]$lines.Add("};")
[void]$lines.Add("")

$resolved = Resolve-Path (Split-Path $OutputPath -Parent) -ErrorAction SilentlyContinue
if (-not $resolved) { New-Item -ItemType Directory -Path (Split-Path $OutputPath -Parent) -Force | Out-Null }
Set-Content -Path $OutputPath -Value ($lines -join "`n") -Encoding ascii
Write-Host "Wrote $OutputPath ($($codepoints.Count) glyphs, height=$fontHeight)"
