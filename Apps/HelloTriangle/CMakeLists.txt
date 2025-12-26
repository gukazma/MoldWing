cmake_minimum_required(VERSION 3.20)

project(HelloTriangle CXX)

set(SOURCE_FILES
    src/HelloTriangle.cpp
)

set(HEADER_FILES
    src/HelloTriangle.hpp
)

# Create executable using DiligentSamples' platform abstraction
add_target_platform_app(HelloTriangle "${SOURCE_FILES}" "${HEADER_FILES}" "")

target_include_directories(HelloTriangle PRIVATE src)

# Link against DiligentSamples infrastructure
target_link_libraries(HelloTriangle
PRIVATE
    Diligent-NativeAppBase
    Diligent-BuildSettings
    Diligent-SampleBase
)

set_common_target_properties(HelloTriangle)

# Platform-specific settings
if(PLATFORM_WIN32)
    set_target_properties(HelloTriangle PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    copy_required_dlls(HelloTriangle)
    append_sample_base_win32_source(HelloTriangle)
endif()

if(MSVC)
    target_compile_options(HelloTriangle PRIVATE /wd4201)
endif()

# Set folder in IDE
set_target_properties(HelloTriangle PROPERTIES FOLDER "MoldWing/Apps")

source_group("src" FILES ${SOURCE_FILES} ${HEADER_FILES})
