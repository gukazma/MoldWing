cmake_minimum_required(VERSION 3.20)

# Project name
project(MoldWing VERSION 0.1 LANGUAGES CXX)

# =============================================================================
# Global Settings
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# DiligentEngine Integration
# =============================================================================
# Configure DiligentEngine options before adding subdirectory
set(DILIGENT_BUILD_SAMPLES ON CACHE BOOL "Build DiligentSamples module")
set(DILIGENT_BUILD_TOOLS ON CACHE BOOL "Build DiligentTools module")
set(DILIGENT_BUILD_FX ON CACHE BOOL "Build DiligentFX module")

# Add DiligentEngine
add_subdirectory(ThirdParty/DiligentEngine)

# =============================================================================
# MoldWing Applications
# =============================================================================
add_subdirectory(Apps)

# =============================================================================
# Tests (optional)
# =============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(Tests)
endif()

# =============================================================================
# Set startup project
# =============================================================================
if(TARGET HelloTriangle)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT HelloTriangle)
elseif(TARGET Tutorial01_HelloTriangle)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Tutorial01_HelloTriangle)
endif()
