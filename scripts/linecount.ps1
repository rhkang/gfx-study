Param(
    $scriptDir = $PSScriptRoot,
    $projectRoot = (Resolve-Path "$scriptDir\..").Path,
    $targetDirs = @("$projectRoot\src", "$projectRoot\sandbox", "$projectRoot\shaders")
)

$lineCount = 0
$files = Get-ChildItem -Path $targetDirs -Recurse -Include *.cpp, *.h, *.hpp, *.hlsl | Sort-Object FullName

foreach ($file in $files) {
    $relativePath = $file.FullName.Substring($projectRoot.Length + 1)
    $line = Get-Content $file.FullName | Measure-Object -Line
    Write-Host ("{0,-50} : {1,5}" -f $relativePath, $line.Lines)
    $lineCount += $line.Lines
}

Write-Host "`nTotal line count: $lineCount`n"