Param(
    $scriptRoot = $PSScriptRoot,
    $projectRoot = (Resolve-Path "$scriptRoot\..").Path
)

# Create CMake build directory
cmake -S $projectRoot -B $projectRoot\build

# Build all targets
cmake --build $projectRoot\build