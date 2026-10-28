Add-Type -AssemblyName System.Drawing
$ErrorActionPreference='Stop'
$taskRepo=Split-Path $PSScriptRoot -Parent
$src=New-Object Drawing.Bitmap((Join-Path $taskRepo 'art/maps/waypoints-20261028/cave-waystone-generated.png'))
if($src.Width -ne $src.Height -or $src.Width -lt 1024) { throw 'Unexpected stone master shape' }
$k=$src.Width/1280.0
$out=New-Object Drawing.Bitmap(256,178,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[Drawing.Graphics]::FromImage($out)
$g.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
$g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($src,(New-Object Drawing.Rectangle(0,0,256,178)),[int](40*$k),[int](270*$k),[int](1180*$k),[int](820*$k),[Drawing.GraphicsUnit]::Pixel)
$out.Save((Join-Path $taskRepo 'assets/maps/cave-waystone.png'),[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$src.Dispose();$out.Dispose()
Write-Output 'Prepared cave waystone, 256x178 RGBA.'
