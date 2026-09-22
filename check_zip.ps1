[Console]::OutputEncoding = [System.Text.Encoding]::UTF8;
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead("LE3B_12_ササナミ_ソウシ.zip")
$entries = $zip.Entries
$sorted = $entries | Sort-Object Length -Descending | Select-Object Name, FullName, Length -First 50
foreach ($entry in $sorted) {
    $sizeMB = [math]::Round($entry.Length / 1MB, 2)
    Write-Host "$sizeMB MB - $($entry.FullName)"
}
$zip.Dispose()
