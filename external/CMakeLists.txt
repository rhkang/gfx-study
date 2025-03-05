set(THIRDPARTY_DIR "thirdparty")

find_package(Vulkan REQUIRED)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

message(STATUS "Assimp install = ${ASSIMP_INSTALL}")

add_subdirectory(assimp)

add_subdirectory(vma)
add_subdirectory(spdlog)
add_subdirectory(glm)
add_subdirectory(glfw)

set(IMGUI_PATH "${CMAKE_SOURCE_DIR}/external/imgui")
set(IMGUI_INCLUDE_DIRS ${IMGUI_PATH} ${IMGUI_PATH}/backends)

file(GLOB IMGUI_SRC CONFIGURE_DEPENDS
    ${IMGUI_PATH}/*.h
    ${IMGUI_PATH}/*.cpp
    ${IMGUI_PATH}/backends/imgui_impl_vulkan.h
    ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_PATH}/backends/imgui_impl_glfw.h
    ${IMGUI_PATH}/backends/imgui_impl_glfw.cpp
)
add_library("imgui" STATIC ${IMGUI_SRC})
target_include_directories("imgui" PUBLIC ${IMGUI_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIR})
target_link_libraries("imgui" PUBLIC
  glfw ${Vulkan_LIBRARIES}
)
