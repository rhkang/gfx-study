# Gfx study

## Build

```
git clone --recursive https://github.com/rhkang/gfx-study.git
git submodule init
git submodule update
```

Install SDL2
```
cd external/sdl
mkdir build
cd build
cmake ..
cmake --build . --config Release
cmake --install .
```

Build
```
cmake -S . -B build

// debug
cmake --build build

// release
cmake --build build --config Release
```

## Vulkan
Required Version : 1.3 or above

Required Features
- samplerAnisotropy
- dynamicRendering
- sampleRateShading
