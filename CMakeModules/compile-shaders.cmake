# compile-shaders.cmake
# CMake module for compiling GLSL/HLSL shaders to SPIR-V at build time

# Find glslc compiler (comes with Vulkan SDK)
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    DOC "Path to glslc shader compiler"
)

# Find glslangValidator as fallback
find_program(GLSLANG_EXECUTABLE
    NAMES glslangValidator
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    DOC "Path to glslangValidator shader compiler"
)

if(GLSLC_EXECUTABLE)
    message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")
    set(SHADER_COMPILER ${GLSLC_EXECUTABLE})
    set(SHADER_COMPILER_TYPE "glslc")
elseif(GLSLANG_EXECUTABLE)
    message(STATUS "Found glslangValidator: ${GLSLANG_EXECUTABLE}")
    set(SHADER_COMPILER ${GLSLANG_EXECUTABLE})
    set(SHADER_COMPILER_TYPE "glslangValidator")
else()
    message(WARNING "No shader compiler found! Shaders will not be compiled.")
    message(WARNING "Please install Vulkan SDK or set VULKAN_SDK environment variable.")
endif()

#[[
    compile_shaders(TARGET target_name
                    SOURCES shader1.vert shader2.frag ...
                    [OUTPUT_DIR output_directory]
                    [INCLUDE_DIRS include_dir1 include_dir2 ...]
                    [DEFINITIONS DEFINE1 DEFINE2 ...]
                    [OPTIMIZE]
    )

    Compiles shader source files to SPIR-V format.

    Arguments:
        TARGET       - Name of the target that will depend on compiled shaders
        SOURCES      - List of shader source files
        OUTPUT_DIR   - Output directory for compiled .spv files (default: ${CMAKE_CURRENT_BINARY_DIR}/shaders)
        INCLUDE_DIRS - Include directories for shader compilation
        DEFINITIONS  - Preprocessor definitions
        OPTIMIZE     - Enable optimization (default: OFF for Debug, ON for Release)
]]
function(compile_shaders)
    if(NOT SHADER_COMPILER)
        message(WARNING "Shader compiler not available, skipping shader compilation")
        return()
    endif()

    # Parse arguments
    set(options OPTIMIZE)
    set(oneValueArgs TARGET OUTPUT_DIR)
    set(multiValueArgs SOURCES INCLUDE_DIRS DEFINITIONS)
    cmake_parse_arguments(SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT SHADER_TARGET)
        message(FATAL_ERROR "compile_shaders: TARGET argument is required")
    endif()

    if(NOT SHADER_SOURCES)
        message(FATAL_ERROR "compile_shaders: SOURCES argument is required")
    endif()

    # Set default output directory
    if(NOT SHADER_OUTPUT_DIR)
        set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    endif()

    # Create output directory
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    # Determine optimization flag
    set(OPTIMIZE_FLAG "")
    if(SHADER_OPTIMIZE OR (CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        if(SHADER_COMPILER_TYPE STREQUAL "glslc")
            set(OPTIMIZE_FLAG "-O")
        else()
            set(OPTIMIZE_FLAG "-Os")
        endif()
    endif()

    # Build include directories flags
    set(INCLUDE_FLAGS "")
    foreach(INCLUDE_DIR ${SHADER_INCLUDE_DIRS})
        list(APPEND INCLUDE_FLAGS "-I${INCLUDE_DIR}")
    endforeach()

    # Build definitions flags
    set(DEFINITION_FLAGS "")
    foreach(DEFINITION ${SHADER_DEFINITIONS})
        if(SHADER_COMPILER_TYPE STREQUAL "glslc")
            list(APPEND DEFINITION_FLAGS "-D${DEFINITION}")
        else()
            list(APPEND DEFINITION_FLAGS "-D${DEFINITION}")
        endif()
    endforeach()

    set(COMPILED_SHADERS "")

    # Compile each shader
    foreach(SHADER_SOURCE ${SHADER_SOURCES})
        # Get absolute path
        get_filename_component(SHADER_ABS_PATH ${SHADER_SOURCE} ABSOLUTE)

        # Get filename
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)

        # Output file path
        set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

        # Add custom command to compile shader
        if(SHADER_COMPILER_TYPE STREQUAL "glslc")
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${SHADER_COMPILER}
                    ${OPTIMIZE_FLAG}
                    ${INCLUDE_FLAGS}
                    ${DEFINITION_FLAGS}
                    -o ${SHADER_OUTPUT}
                    ${SHADER_ABS_PATH}
                DEPENDS ${SHADER_ABS_PATH}
                COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
                VERBATIM
            )
        else()
            # glslangValidator syntax
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${SHADER_COMPILER}
                    ${OPTIMIZE_FLAG}
                    ${INCLUDE_FLAGS}
                    ${DEFINITION_FLAGS}
                    -V
                    -o ${SHADER_OUTPUT}
                    ${SHADER_ABS_PATH}
                DEPENDS ${SHADER_ABS_PATH}
                COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
                VERBATIM
            )
        endif()

        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
    endforeach()

    # Create a custom target for compiled shaders
    set(SHADER_TARGET_NAME "${SHADER_TARGET}_shaders")
    add_custom_target(${SHADER_TARGET_NAME}
        DEPENDS ${COMPILED_SHADERS}
        COMMENT "Compiling all shaders for ${SHADER_TARGET}"
    )

    # Make the main target depend on shader compilation
    add_dependencies(${SHADER_TARGET} ${SHADER_TARGET_NAME})

    # Store output directory in a property for later use
    set_target_properties(${SHADER_TARGET} PROPERTIES
        SHADER_OUTPUT_DIR ${SHADER_OUTPUT_DIR}
    )

    message(STATUS "Configured shader compilation for target: ${SHADER_TARGET}")
    message(STATUS "  - Shaders: ${SHADER_SOURCES}")
    message(STATUS "  - Output: ${SHADER_OUTPUT_DIR}")
endfunction()

#[[
    Simple wrapper for single shader compilation

    compile_shader(shader.vert OUTPUT shader.vert.spv)
]]
function(compile_shader SHADER_SOURCE)
    cmake_parse_arguments(SHADER "" "OUTPUT" "" ${ARGN})

    if(NOT SHADER_OUTPUT)
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")
    endif()

    get_filename_component(SHADER_ABS_PATH ${SHADER_SOURCE} ABSOLUTE)

    if(SHADER_COMPILER_TYPE STREQUAL "glslc")
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${SHADER_COMPILER}
                -o ${SHADER_OUTPUT}
                ${SHADER_ABS_PATH}
            DEPENDS ${SHADER_ABS_PATH}
            COMMENT "Compiling shader: ${SHADER_SOURCE}"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${SHADER_COMPILER}
                -V
                -o ${SHADER_OUTPUT}
                ${SHADER_ABS_PATH}
            DEPENDS ${SHADER_ABS_PATH}
            COMMENT "Compiling shader: ${SHADER_SOURCE}"
            VERBATIM
        )
    endif()
endfunction()

#[[
    compile_shaders_to_headers(TARGET target_name
                                SOURCES shader1.vert shader2.frag ...
                                [OUTPUT_DIR output_directory]
                                [NAMESPACE namespace_name]
                                [INCLUDE_DIRS include_dir1 ...]
                                [DEFINITIONS DEFINE1 DEFINE2 ...]
                                [OPTIMIZE]
    )

    Compiles shaders to SPIR-V and generates C++ header files for embedded shaders.

    Arguments:
        TARGET       - Name of the target that will depend on compiled shaders
        SOURCES      - List of shader source files
        OUTPUT_DIR   - Output directory for .h files (default: ${CMAKE_CURRENT_BINARY_DIR}/shaders)
        NAMESPACE    - C++ namespace for shader data (default: Shaders)
        INCLUDE_DIRS - Include directories for shader compilation
        DEFINITIONS  - Preprocessor definitions
        OPTIMIZE     - Enable optimization
]]
function(compile_shaders_to_headers)
    if(NOT SHADER_COMPILER)
        message(WARNING "Shader compiler not available, skipping shader compilation")
        return()
    endif()

    # Find Python for header generation
    find_package(Python3 COMPONENTS Interpreter)
    if(NOT Python3_FOUND)
        message(FATAL_ERROR "Python3 is required for shader-to-header conversion")
    endif()

    # Parse arguments
    set(options OPTIMIZE)
    set(oneValueArgs TARGET OUTPUT_DIR NAMESPACE)
    set(multiValueArgs SOURCES INCLUDE_DIRS DEFINITIONS)
    cmake_parse_arguments(SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT SHADER_TARGET)
        message(FATAL_ERROR "compile_shaders_to_headers: TARGET argument is required")
    endif()

    if(NOT SHADER_SOURCES)
        message(FATAL_ERROR "compile_shaders_to_headers: SOURCES argument is required")
    endif()

    # Set default output directory
    if(NOT SHADER_OUTPUT_DIR)
        set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    endif()

    # Set default namespace
    if(NOT SHADER_NAMESPACE)
        set(SHADER_NAMESPACE "Shaders")
    endif()

    # Create output directory
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    # Determine optimization flag
    set(OPTIMIZE_FLAG "")
    if(SHADER_OPTIMIZE OR (CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        if(SHADER_COMPILER_TYPE STREQUAL "glslc")
            set(OPTIMIZE_FLAG "-O")
        else()
            set(OPTIMIZE_FLAG "-Os")
        endif()
    endif()

    # Build include directories flags
    set(INCLUDE_FLAGS "")
    foreach(INCLUDE_DIR ${SHADER_INCLUDE_DIRS})
        list(APPEND INCLUDE_FLAGS "-I${INCLUDE_DIR}")
    endforeach()

    # Build definitions flags
    set(DEFINITION_FLAGS "")
    foreach(DEFINITION ${SHADER_DEFINITIONS})
        list(APPEND DEFINITION_FLAGS "-D${DEFINITION}")
    endforeach()

    set(GENERATED_HEADERS "")
    set(SPV_TO_HEADER_SCRIPT "${CMAKE_SOURCE_DIR}/CMakeModules/spv_to_header.py")

    # Compile each shader and generate header
    foreach(SHADER_SOURCE ${SHADER_SOURCES})
        # Get absolute path
        get_filename_component(SHADER_ABS_PATH ${SHADER_SOURCE} ABSOLUTE)

        # Get filename and create variable name
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        string(REPLACE "." "_" VAR_NAME ${SHADER_NAME})

        # Output paths
        set(SPV_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
        set(HEADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.h")

        # Step 1: Compile shader to SPIR-V
        if(SHADER_COMPILER_TYPE STREQUAL "glslc")
            add_custom_command(
                OUTPUT ${SPV_OUTPUT}
                COMMAND ${SHADER_COMPILER}
                    ${OPTIMIZE_FLAG}
                    ${INCLUDE_FLAGS}
                    ${DEFINITION_FLAGS}
                    -o ${SPV_OUTPUT}
                    ${SHADER_ABS_PATH}
                DEPENDS ${SHADER_ABS_PATH}
                COMMENT "Compiling shader to SPIR-V: ${SHADER_NAME}"
                VERBATIM
            )
        else()
            # glslangValidator syntax
            add_custom_command(
                OUTPUT ${SPV_OUTPUT}
                COMMAND ${SHADER_COMPILER}
                    ${OPTIMIZE_FLAG}
                    ${INCLUDE_FLAGS}
                    ${DEFINITION_FLAGS}
                    -V
                    -o ${SPV_OUTPUT}
                    ${SHADER_ABS_PATH}
                DEPENDS ${SHADER_ABS_PATH}
                COMMENT "Compiling shader to SPIR-V: ${SHADER_NAME}"
                VERBATIM
            )
        endif()

        # Step 2: Convert SPIR-V to C++ header
        add_custom_command(
            OUTPUT ${HEADER_OUTPUT}
            COMMAND ${Python3_EXECUTABLE}
                ${SPV_TO_HEADER_SCRIPT}
                ${SPV_OUTPUT}
                ${HEADER_OUTPUT}
                ${VAR_NAME}
            DEPENDS ${SPV_OUTPUT} ${SPV_TO_HEADER_SCRIPT}
            COMMENT "Generating header: ${SHADER_NAME}.h"
            VERBATIM
        )

        list(APPEND GENERATED_HEADERS ${HEADER_OUTPUT})
    endforeach()

    # Create a custom target for shader headers
    set(SHADER_TARGET_NAME "${SHADER_TARGET}_shaders")
    add_custom_target(${SHADER_TARGET_NAME}
        DEPENDS ${GENERATED_HEADERS}
        COMMENT "Generating all shader headers for ${SHADER_TARGET}"
    )

    # Make the main target depend on shader compilation
    add_dependencies(${SHADER_TARGET} ${SHADER_TARGET_NAME})

    # Add include directory for generated headers
    target_include_directories(${SHADER_TARGET} PRIVATE ${SHADER_OUTPUT_DIR})

    # Store output directory in a property for later use
    set_target_properties(${SHADER_TARGET} PROPERTIES
        SHADER_HEADER_DIR ${SHADER_OUTPUT_DIR}
    )

    message(STATUS "Configured shader-to-header compilation for target: ${SHADER_TARGET}")
    message(STATUS "  - Shaders: ${SHADER_SOURCES}")
    message(STATUS "  - Headers output: ${SHADER_OUTPUT_DIR}")
endfunction()
