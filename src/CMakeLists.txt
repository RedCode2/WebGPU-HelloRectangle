add_executable(main main.cxx)

set_target_properties(main PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(main PRIVATE /W4)
else()
    target_compile_options(main PRIVATE -Wall -Wextra -pedantic)
endif()

if (XCODE)
    set_target_properties(main PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
    )
endif()

# Enable the use of emscripten_sleep()
target_link_options(main PRIVATE -sASYNCIFY)

target_link_libraries(main PRIVATE glfw)
target_link_libraries(main PRIVATE webgpu)
target_link_libraries(main PRIVATE glfw3webgpu)

find_package(OpenGL REQUIRED)
target_link_libraries(main PRIVATE OpenGL::GL)
