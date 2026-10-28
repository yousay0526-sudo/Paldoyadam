param([string]$RepositoryRoot = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Drawing2D;using System.Collections.Generic;
public class ApprovedWaypointPrep {
 public static Rectangle Bounds(Bitmap b,Rectangle region) {
  int left=region.Right,top=region.Bottom,right=-1,bottom=-1;
  for(int y=region.Top;y<region.Bottom;y++)for(int x=region.Left;x<region.Right;x++)if(b.GetPixel(x,y).A>8) {left=Math.Min(left,x);top=Math.Min(top,y);right=Math.Max(right,x);bottom=Math.Max(bottom,y);}
  if(right<left||bottom<top)throw new Exception("Empty sprite region");
  return Rectangle.FromLTRB(Math.Max(region.Left,left-2),Math.Max(region.Top,top-2),Math.Min(region.Right,right+3),Math.Min(region.Bottom,bottom+3));
 }
 public static void Copy(Bitmap source,Rectangle box,Bitmap target,Rectangle slot,bool feet) {
  float scale=Math.Min((slot.Width-8f)/box.Width,(slot.Height-8f)/box.Height);
  float width=box.Width*scale,height=box.Height*scale;
  RectangleF to=new RectangleF(slot.X+(slot.Width-width)/2,feet?slot.Bottom-8-height:slot.Y+(slot.Height-height)/2,width,height);
  using(Graphics g=Graphics.FromImage(target)) {g.CompositingMode=CompositingMode.SourceCopy;g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(source,to,box,GraphicsUnit.Pixel);}
 }
 public static string[] Run(string sourcePath,string boardsPath,string rockPath) {
  using(Bitmap source=new Bitmap(sourcePath))using(Bitmap boards=new Bitmap(1152,100,System.Drawing.Imaging.PixelFormat.Format32bppArgb))using(Bitmap rock=new Bitmap(256,178,System.Drawing.Imaging.PixelFormat.Format32bppArgb)) {
   int half=source.Width/2,cut=(int)(source.Height*.44),lower=source.Height/2;
   Rectangle[] regions={new Rectangle(0,0,half,cut),new Rectangle(half,0,source.Width-half,cut),new Rectangle(0,lower,half,source.Height-lower),new Rectangle(half,cut,source.Width-half,source.Height-cut)};
   List<string> records=new List<string>();
   for(int i=0;i<4;i++) {Rectangle b=Bounds(source,regions[i]);if(b.Width<100||b.Height<50)throw new Exception("Unexpected sprite bounds");Copy(source,b,i<3?boards:rock,i<3?new Rectangle(i*384,0,384,100):new Rectangle(0,0,256,178),i==3);records.Add(i+":"+b.X+","+b.Y+","+b.Width+","+b.Height);}
   for(int i=0;i<3;i++)if(boards.GetPixel(i*384,0).A!=0||boards.GetPixel(i*384+383,99).A!=0)throw new Exception("Board alpha gutter missing");
   if(rock.GetPixel(0,0).A!=0||rock.GetPixel(255,177).A!=0)throw new Exception("Rock alpha border missing");
   boards.Save(boardsPath,System.Drawing.Imaging.ImageFormat.Png);rock.Save(rockPath,System.Drawing.Imaging.ImageFormat.Png);return records.ToArray();
  }
 }
}
'@
$art=Join-Path $RepositoryRoot 'art/maps/waypoints-20261028'
$records=[ApprovedWaypointPrep]::Run((Join-Path $art 'approved-material-atlas.png'),(Join-Path $RepositoryRoot 'assets/maps/waypost-boards.png'),(Join-Path $RepositoryRoot 'assets/maps/cave-waystone.png'))
$metadata=[ordered]@{source='approved-material-atlas.png';source_size=@(1774,887);boards=[ordered]@{size=@(1152,100);cell_size=@(384,100);names=@('솔바람재','탐식굴','마을')};stone=[ordered]@{size=@(256,178);name='마을';arrow=$false;painted_fill=$false};source_bounds=$records;method='alpha > 8 bounds, 2px source padding, aspect-preserving bicubic fit, transparent gutters; stone aligned to lower foot point'}
$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $art 'approved-preparation.json') -Encoding UTF8
$records
Write-Output 'Prepared authored board lettering and carved stone; no game execution.'
