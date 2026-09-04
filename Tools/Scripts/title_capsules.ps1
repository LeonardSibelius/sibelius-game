# title_capsules.ps1 - put the game's name on the five rendered capsules.
#
# Reads  Saved/MovieRenders/Capsules/*.png   (Kaia at frame 760, alpha background)
# Writes Saved/MovieRenders/CapsulesTitled/  (flattened onto black, JPG, titled)
#
# ONE LINE, ONE WEIGHT (Walt, 2026-09-03): "why is SIBELIUS so big? Can't Leonard
# Sibelius all be in the same font as Leonard and all on the same line?"
#
# The first draft stacked a small LEONARD over a large bold SIBELIUS - a film-poster
# treatment. This is quieter and reads more like a name than a logo, which suits a game
# about a man's career.
#
# The renders carry an ALPHA background, so each is composited onto solid black first -
# Steam capsules must not ship with transparency.
#
# AUTO-FIT: the type shrinks until it fits the space available, rather than running into
# her face or off the edge. Each capsule declares how much width the title may use.

Add-Type -AssemblyName System.Drawing

$srcDir = "C:\Users\wpark\projects\sibelius-game\Saved\MovieRenders\Capsules"
$outDir = "C:\Users\wpark\projects\sibelius-game\Saved\MovieRenders\CapsulesTitled"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

$jpg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ep  = New-Object System.Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, 94)

$TITLE = "LEONARD SIBELIUS"

function Measure-Tracked {
    param($g, [string]$text, $font, [single]$track)
    $w = 0
    foreach ($ch in $text.ToCharArray()) { $w += $g.MeasureString([string]$ch, $font).Width - 6 + $track }
    return $w
}

function Draw-Tracked {
    param($g, [string]$text, $font, $brush, [single]$x, [single]$y, [single]$track)
    $cx = $x
    foreach ($ch in $text.ToCharArray()) {
        $g.DrawString([string]$ch, $font, $brush, $cx, $y)
        $cx += $g.MeasureString([string]$ch, $font).Width - 6 + $track
    }
}

# pt = starting size, shrunk if needed. maxW = fraction of the width the title may use.
$plan = @(
  @{ f='capsule_header_920x430.png';   pt=26; track=5; place='left';   maxW=0.36 },
  @{ f='capsule_small_462x174.png';    pt=40; track=4; place='centre'; maxW=0.92 },
  @{ f='capsule_main_1232x706.png';    pt=34; track=7; place='left';   maxW=0.34 },
  @{ f='capsule_vertical_748x896.png'; pt=30; track=5; place='top';    maxW=0.88 },
  @{ f='capsule_library_600x900.png';  pt=26; track=4; place='top';    maxW=0.88 }
)

foreach ($p in $plan) {
    $src = Join-Path $srcDir $p.f
    if (-not (Test-Path $src)) { Write-Output "  MISSING $($p.f)"; continue }

    $img = [System.Drawing.Image]::FromFile($src)
    $w = $img.Width; $h = $img.Height

    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    $g.DrawImage($img, 0, 0, $w, $h)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

    # Shrink until it fits the allowance. Better a slightly smaller name than one
    # colliding with her hair.
    $pt = [single]$p.pt
    $track = [single]$p.track
    $limit = $w * $p.maxW
    do {
        $font = New-Object System.Drawing.Font("Segoe UI", $pt, [System.Drawing.FontStyle]::Regular)
        $tw = Measure-Tracked $g $TITLE $font $track
        if ($tw -le $limit -or $pt -le 8) { break }
        $font.Dispose(); $pt -= 1; $track = [math]::Max(1, $track - 0.15)
    } while ($true)

    $white  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(240,240,240))
    $shadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(170,0,0,0))

    switch ($p.place) {
        'left'   { $x = $w * 0.05;        $y = $h * 0.42 }
        'centre' { $x = ($w - $tw) / 2;   $y = $h * 0.13 }
        'top'    { $x = ($w - $tw) / 2;   $y = $h * 0.05 }
    }

    Draw-Tracked $g $TITLE $font $shadow ($x+2) ($y+2) $track
    Draw-Tracked $g $TITLE $font $white  $x     $y     $track

    $out = Join-Path $outDir ($p.f -replace '\.png$', '.jpg')
    $bmp.Save($out, $jpg, $ep)
    "  {0,-34} {1,4}x{2,-4}  title {3}pt, {4:N0}px wide" -f (Split-Path $out -Leaf), $w, $h, $pt, $tw

    $g.Dispose(); $bmp.Dispose(); $img.Dispose(); $font.Dispose()
}

# THE LEGIBILITY TEST: the small capsule as Steam actually shows it.
$smallOut = Join-Path $outDir "capsule_small_462x174.jpg"
if (Test-Path $smallOut) {
    $s = [System.Drawing.Image]::FromFile($smallOut)
    $t = New-Object System.Drawing.Bitmap 120, 45
    $tg = [System.Drawing.Graphics]::FromImage($t)
    $tg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $tg.DrawImage($s, 0, 0, 120, 45)
    $tg.Dispose()
    $t.Save((Join-Path $outDir "THUMBNAIL_TEST_120x45.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $t.Dispose(); $s.Dispose()
    Write-Output ""
    Write-Output "  THUMBNAIL_TEST_120x45.png written - this is the readability check"
}
Write-Output ""
Write-Output "output: $outDir"
