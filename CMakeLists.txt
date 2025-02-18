cmake_minimum_required(VERSION 3.20)

set(ENGINE_NAME "Engine")
project(${ENGINE_NAME} VERSION 0.1.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

#######################
### Source Codes
#######################

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
"${CMAKE_SOURCE_DIR}/src/*.h"
"${CMAKE_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
"${CMAKE_SOURCE_DIR}/src/*.cpp"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
add_library(${ENGINE_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${ENGINE_NAME}
PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    VulkanMemoryAllocator 
    ${CMAKE_SOURCE_DIR}/external/stb_image
    ${GLFW_INCLUDE_DIR}
    assimp::assimp
)
target_link_libraries(${ENGINE_NAME}
PRIVATE
PUBLIC
    ${Vulkan_LIBRARIES}
    GPUOpen::VulkanMemoryAllocator
    spdlog::spdlog
    imgui
    glm
    glfw
    assimp::assimp
    ${SDL2_LIBRARIES}
)

set_target_properties(${ENGINE_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    # COMPILE_WARNING_AS_ERROR ON
)

add_compile_definitions(SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/out/")
add_compile_definitions(RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources/")
add_compile_definitions(SDL_MAIN_HANDLED=)
add_compile_definitions(GLM_FORCE_RADIANS=)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE=)

if (WIN32)
    # Apply /EHsc for exception handling in Visual Studio (MSVC compiler)
    target_compile_options(${ENGINE_NAME} PRIVATE /EHsc /wd26495 /wd28251)
    add_compile_definitions(NOMINMAX=)
endif()

add_subdirectory(sandbox)