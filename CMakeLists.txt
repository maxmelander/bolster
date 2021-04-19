cmake_minimum_required(VERSION 3.18)

project(vulkantest)

file(GLOB bolster_Sources CONFIGURE_DEPENDS "src/*.cpp")

file(GLOB soloud_Sources
  "d:/soloud20200207/src/core/*.cpp"
  "d:/soloud20200207/src/backend/wasapi/*.cpp"
  "d:/soloud20200207/src/audiosource/wav/*.cpp"
  )
file(GLOB soloud_C_Sources CONFIGURE_DEPENDS
  "d:/soloud20200207/src/audiosource/wav/*.c"
  )

add_executable(vulkantest ${bolster_Sources} ${soloud_Sources} ${soloud_C_Sources})

include_directories(${PROJECT_SOURCE_DIR}/includes)
include_directories("d:/soloud20200207/include")

# Compile options
set(CMAKE_CXX_FLAGS "-std=c++17")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Other options
# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("d:/glfw" "glfw")

# GLM
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory("extern/glm" "glm")

# Includes
include_directories("extern")
include_directories("extern/vma/src")
include_directories("extern/stb")
include_directories("extern/tiny_obj")

# TODO: Add as git submodule
# include_directories("d:/soloud20200207/include")

# VULKAN
find_package(Vulkan REQUIRED)
target_compile_definitions(vulkantest PRIVATE VK_USE_PLATFORM_WIN32_KHR)


target_link_libraries(vulkantest Vulkan::Vulkan glfw glm)

