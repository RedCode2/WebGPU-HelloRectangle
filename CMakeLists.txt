cmake_minimum_required(VERSION 3.10)
project(
    WebGPULearning
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src)
add_subdirectory(thirdparty/glfw-3.4/glfw-3.4)
add_subdirectory(thirdparty/WebGPU-distribution-wgpu-v0.19.4.1/WebGPU-distribution-wgpu-v0.19.4.1)
add_subdirectory(thirdparty/glfw3webgpu)