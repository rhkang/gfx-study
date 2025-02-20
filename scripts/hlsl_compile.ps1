Param(
    $currentDirectory = $PSScriptRoot,
    $projectRoot = (Resolve-Path "$currentDirectory\..").Path,
    $shaderRoot = "$projectRoot\shaders"
)

$arch = if ([System.Environment]::Is64BitProcess) { "x64" } else { "x86" }
$dxc = "$projectRoot\tool\dxc\win\bin\$arch\dxc.exe"

Write-Host "Compiler: $dxc"
Write-Host "Shader root: $shaderRoot"

$files = Get-ChildItem -Path $shaderRoot -Recurse -Include *.hlsl

foreach ($file in $files) {
    $relativePath = $file.FullName.Substring($shaderRoot.Length + 1)
    $outputPath = "$shaderRoot\out\$relativePath"
    $outputDir = Split-Path $outputPath -Parent
    $basename = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)

    if (-not (Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir -Force
    }

    # execute via command
    Write-Host "Compiling $relativePath"
    & $dxc -spirv -T vs_6_0 -E vert $file.FullName -Fo "$outputDir\$basename.vert.spv"
    & $dxc -spirv -T ps_6_0 -E frag $file.FullName -Fo "$outputDir\$basename.frag.spv"
    Write-Host
}