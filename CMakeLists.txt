cmake_minimum_required(VERSION 3.18)

project(vulkantest)
include_directories(${PROJECT_SOURCE_DIR})
add_executable(vulkantest main.cpp dstack.cpp camera.cpp)

# Compile options
set(CMAKE_CXX_FLAGS "-std=c++17")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("d:/glfw" "glfw")

# GLM
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory("d:/glm" "glm")

# Image loader
include_directories("d:/stb")

# Obj loader
include_directories("d:/tinyobjloader")

# VULKAN
find_package(Vulkan REQUIRED)
target_compile_definitions(vulkantest PRIVATE VK_USE_PLATFORM_WIN32_KHR)

# set(SOURCE_FILES main.cpp dstack.cpp)

target_link_libraries(vulkantest Vulkan::Vulkan glfw glm)

