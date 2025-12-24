set(TARGET_NAME Shaders)

# Find Python3
find_package(Python3 REQUIRED)

# Find glslc compiler
find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found! Please install Vulkan SDK")
endif()

message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")

# SPIR-V to header conversion script
set(SPV_TO_HEADER_SCRIPT "${CMAKE_SOURCE_DIR}/CMakeModules/spv_to_header.py")

# Output directory for compiled shader headers
set(SHADER_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/MoldWing/Shaders)
file(MAKE_DIRECTORY ${SHADER_HEADER_DIR})

# List all shader files from the shaders directory
file(GLOB SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
)

# Function to compile shader to SPIR-V and generate C++ header
set(SHADER_HEADERS "")
foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    get_filename_component(SHADER_EXT ${SHADER_FILE} EXT)

    # Create variable name from shader name
    string(REPLACE "." "_" VAR_NAME ${SHADER_NAME})

    # SPIR-V output
    set(SPIRV_FILE ${SHADER_HEADER_DIR}/${SHADER_NAME}.spv)

    # C++ header output
    set(HEADER_FILE ${SHADER_HEADER_DIR}/${SHADER_NAME}.h)

    # Set optimization flag based on build type
    set(GLSLC_OPTS "")
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(GLSLC_OPTS "-O")
    endif()

    # Add Vulkan 1.2 target environment for compute shaders with ray query extension
    if(SHADER_EXT STREQUAL ".comp")
        list(APPEND GLSLC_OPTS "--target-env=vulkan1.2")
    endif()

    # Compile shader to SPIR-V
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_FILE} -o ${SPIRV_FILE} ${GLSLC_OPTS}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling shader: ${SHADER_NAME}"
        VERBATIM
    )

    # Generate C++ header from SPIR-V
    add_custom_command(
        OUTPUT ${HEADER_FILE}
        COMMAND ${Python3_EXECUTABLE}
            ${SPV_TO_HEADER_SCRIPT}
            ${SPIRV_FILE}
            ${HEADER_FILE}
            ${VAR_NAME}
        DEPENDS ${SPIRV_FILE} ${SPV_TO_HEADER_SCRIPT}
        COMMENT "Generating header: ${SHADER_NAME}.h"
        VERBATIM
    )

    list(APPEND SHADER_HEADERS ${HEADER_FILE})
endforeach()

# Create interface library
add_library(${TARGET_NAME} INTERFACE)

# Add custom target for shader compilation
add_custom_target(${TARGET_NAME}_generate ALL DEPENDS ${SHADER_HEADERS})
set_target_properties(${TARGET_NAME}_generate PROPERTIES FOLDER Modules/MoldWing/Shaders)

# Add include directory
target_include_directories(${TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Create alias
add_library(MoldWing::Shaders ALIAS ${TARGET_NAME})

# Install interface library
install(TARGETS ${TARGET_NAME}
    EXPORT ShadersTargets
)

# Install shader headers
install(DIRECTORY ${SHADER_HEADER_DIR}/
        DESTINATION include/MoldWing/Shaders
        FILES_MATCHING PATTERN "*.h"
)

# Install export configuration
install(EXPORT ShadersTargets
    DESTINATION cmake/Shaders
    FILE Shaders-config.cmake
    NAMESPACE MoldWing::
)

