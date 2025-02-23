Param(
    $currentDirectory = $PSScriptRoot,
    $projectRoot = (Resolve-Path "$currentDirectory\..").Path,
    $targetDirs = @("$projectRoot\src", "$projectRoot\sandbox")
)

Write-Host "Script directory: $currentDirectory"
Write-Host "Project root: $projectRoot"
Write-Host "Target directories: $targetDirs"

for ($i = 0; $i -lt $targetDirs.Length; $i++) {
    $targetDir = $targetDirs[$i]
    
    # recursively find all .c .h .cpp .hpp files
    $files = Get-ChildItem -Path $targetDir -Recurse -Include *.c, *.h, *.cpp, *.hpp

    foreach ($file in $files) {
        $originalContent = Get-Content $file.FullName -Raw
        clang-format -i --style=file:$projectRoot\.clang-format $file.FullName
        $formattedContent = Get-Content $file.FullName -Raw

        if ($originalContent -ne $formattedContent) {
            Write-Host "Formatted: $file"
        }
    }
}

