param([Parameter(Mandatory=$true)][string]$OriginalVillage)
$ErrorActionPreference='Stop'
if((Get-FileHash -LiteralPath $OriginalVillage -Algorithm SHA256).Hash -ne '8a5988fca8c8eebff354c0addef14f4ecc41edc50b4ca575409a74655574bbcb') { throw 'Use the unchanged approved village from main 535a13e as input.' }
Add-Type -AssemblyName System.Drawing
$repo = Split-Path $PSScriptRoot -Parent
$art = Join-Path $repo 'art/maps/waypoints-20261028'
$base = New-Object Drawing.Bitmap($OriginalVillage)
$edit = New-Object Drawing.Bitmap((Join-Path $art 'village-open-east-generated.png'))
if ($base.Width -ne 1536 -or $base.Height -ne 1024 -or $edit.Size -ne $base.Size) { throw 'Map dimensions must remain 1536x1024.' }
# Feather only the generated eastern gate correction; preserve all other pixels.
for ($y=545; $y -lt 720; $y++) { for ($x=1330; $x -lt 1536; $x++) {
  $a=[Math]::Min(1.0,[Math]::Min(($x-1330)/16.0,[Math]::Min(($y-545)/16.0,(719-$y)/16.0)))
  if ($a -le 0) { continue }
  $p=$base.GetPixel($x,$y); $q=$edit.GetPixel($x,$y)
  $base.SetPixel($x,$y,[Drawing.Color]::FromArgb(255,[int]($p.R*(1-$a)+$q.R*$a),[int]($p.G*(1-$a)+$q.G*$a),[int]($p.B*(1-$a)+$q.B*$a)))
} }
$base.Save((Join-Path $repo 'assets/maps/village.png'),[Drawing.Imaging.ImageFormat]::Png)
$base.Dispose(); $edit.Dispose()
$src=New-Object Drawing.Bitmap((Join-Path $art 'waypost-generated.png'))
$out=New-Object Drawing.Bitmap(256,256,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[Drawing.Graphics]::FromImage($out)
$g.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
$g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($src,(New-Object Drawing.Rectangle(0,0,256,256)))
$out.Save((Join-Path $repo 'assets/maps/waypost.png'),[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $src.Dispose(); $out.Dispose()
Write-Output 'Prepared local gate correction and transparent waypost.'
