Add-Type -AssemblyName System.Drawing
$images = @("aim_cursor.png", "lock_on_reticle.png")
$basePath = "c:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\resources\"

foreach ($img in $images) {
    $path = $basePath + $img
    if (-not (Test-Path $path)) { continue }
    
    $bmp = New-Object System.Drawing.Bitmap $path
    $width = $bmp.Width
    $height = $bmp.Height
    $newBmp = New-Object System.Drawing.Bitmap $width, $height
    
    for ($y = 0; $y -lt $height; $y++) {
        for ($x = 0; $x -lt $width; $x++) {
            $pixel = $bmp.GetPixel($x, $y)
            $maxVal = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
            $newBmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($maxVal, 255, 255, 255))
        }
    }
    
    $bmp.Dispose()
    Remove-Item -Force $path
    $newBmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $newBmp.Dispose()
    Write-Host ("Processed " + $img)
}
