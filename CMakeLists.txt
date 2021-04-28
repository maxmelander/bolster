cmake_minimum_required(VERSION 3.18)

project(vulkantest)

file(GLOB bolster_Sources CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB bolster_C_Sources CONFIGURE_DEPENDS "src/*.c")

file(GLOB soloud_Sources
  "d:/soloud20200207/src/core/*.cpp"
  "d:/soloud20200207/src/backend/wasapi/*.cpp"
  "d:/soloud20200207/src/audiosource/wav/*.cpp"
  )
file(GLOB soloud_C_Sources
  "d:/soloud20200207/src/audiosource/wav/*.c"
  )

add_executable(vulkantest ${bolster_Sources} ${bolster_C_Sources} ${soloud_Sources} ${soloud_C_Sources})

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

# set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
# set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# find_package(assimp 4.1 QUIET)
# if (NOT assimp_FOUND)
    # message(STATUS "Assimp not installed. Compiling from source.")
    # set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
    # add_subdirectory ("extern/assimp")
    #set(assimp_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/assimp/include/" "${CMAKE_CURRENT_BINARY_DIR}/extern/assimp/include/")
# endif (NOT assimp_FOUND)
# include_directories(assimp/include)
# add_subdirectory(extern/assimp)
# add_subdirectory("extern/assimp" "assimp")

# Includes
# 
include_directories("extern")
include_directories("extern/vma/src")
# include_directories("extern/stb")
include_directories("extern/tiny_obj")
include_directories("extern/tinygltf")



# VULKAN
find_package(Vulkan REQUIRED)
target_compile_definitions(vulkantest PRIVATE VK_USE_PLATFORM_WIN32_KHR)


target_link_libraries(vulkantest Vulkan::Vulkan glfw glm)

