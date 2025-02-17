# Gfx study

## Start
```
git clone --recursive https://github.com/rhkang/gfx-study.git
git submodule init
git submodule update
```

### Install
#### SDL2
```
cd external/sdl
mkdir build
cd build
cmake ..
cmake --build . --config Release
cmake --install .
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
- Windows에서 SDL2.dll 못 찾을 경우 설치 경로 환경변수에 추가 (C:\Program Files (x86)\SDL2\bin 확인)
