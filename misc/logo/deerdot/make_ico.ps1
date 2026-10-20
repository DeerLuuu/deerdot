param(
    [Parameter(Mandatory=$true)][string]$Out,
    [Parameter(Mandatory=$true)][string]$PngList
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$Pngs = $PngList -split ';'

$images = @()
foreach ($p in $Pngs) {
    $full = (Resolve-Path $p).Path
    $bmp = [System.Drawing.Image]::FromFile($full)
    $w = $bmp.Width
    $h = $bmp.Height
    $bmp.Dispose()
    $bytes = [System.IO.File]::ReadAllBytes($full)
    $images += [pscustomobject]@{ W = $w; H = $h; Data = $bytes }
}
$images = $images | Sort-Object W

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)
$bw.Write([UInt16]0)
$bw.Write([UInt16]1)
$bw.Write([UInt16]$images.Count)
$offset = 6 + 16 * $images.Count
foreach ($img in $images) {
    $wb = if ($img.W -ge 256) { 0 } else { $img.W }
    $hb = if ($img.H -ge 256) { 0 } else { $img.H }
    $bw.Write([byte]$wb)
    $bw.Write([byte]$hb)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]$img.Data.Length)
    $bw.Write([UInt32]$offset)
    $offset += $img.Data.Length
}
foreach ($img in $images) { $bw.Write($img.Data) }
$bw.Flush()
[System.IO.File]::WriteAllBytes($Out, $ms.ToArray())
$bw.Dispose()
$ms.Dispose()
Write-Output ("wrote {0} ({1} entries: {2})" -f $Out, $images.Count, (($images | ForEach-Object { $_.W }) -join ','))
