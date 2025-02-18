# Gfx study

## Start
```
git clone --recursive https://github.com/rhkang/gfx-study.git
git submodule init
git submodule update
```

### Vulkan
Required SDK Version : 1.3 or above (https://vulkan.lunarg.com/)

Required Features
- samplerAnisotropy
- dynamicRendering
- sampleRateShading

### Build Project
```
cmake -S . -B build

// debug
cmake --build build

// release
cmake --build build --config Release
```

### Shader Compile
```
> scripts/hlsl_compile.bat
```

Currently work on Windows only (hlsl -> SPIR-V)

## Troubleshooting
