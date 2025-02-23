Param(
    $currentDirectory = $PSScriptRoot,
    $projectRoot = (Resolve-Path "$currentDirectory\..").Path,
    $targetDirs = @("$projectRoot\src", "$projectRoot\sandbox")
)

Write-Host "Script root: $currentDirectory"
Write-Host "Project root: $projectRoot"
Write-Host "Target directories: $targetDirs"

for ($i = 0; $i -lt $targetDirs.Length; $i++) {
    $targetDir = $targetDirs[$i]
    
    # recursively find all .c .h .cpp .hpp files
    $files = Get-ChildItem -Path $targetDir -Recurse -Include *.c, *.h, *.cpp, *.hpp

    foreach ($file in $files) {
        Write-Host "Formatting $file"
        clang-format -i --style=file:$projectRoot\.clang-format $file.FullName
    }
}

