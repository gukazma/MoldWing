cmake_minimum_required(VERSION 3.20)

# Disable vcpkg auto-deploy for Qt (we'll handle it manually)
set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "" FORCE)

project(MoldWing CXX)

# =============================================================================
# Find Qt6
# =============================================================================
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

# =============================================================================
# Find assimp
# =============================================================================
find_package(assimp CONFIG REQUIRED)

# =============================================================================
# Source files
# =============================================================================
set(MAIN_SOURCES
    src/main.cpp
    src/MainWindow.cpp
)

set(MAIN_HEADERS
    src/MainWindow.hpp
)

set(RENDER_SOURCES
    src/Render/DiligentWidget.cpp
    src/Render/OrbitCamera.cpp
    src/Render/MeshRenderer.cpp
)

set(RENDER_HEADERS
    src/Render/DiligentWidget.hpp
    src/Render/OrbitCamera.hpp
    src/Render/MeshRenderer.hpp
)

set(CORE_SOURCES
    src/Core/MeshData.cpp
)

set(CORE_HEADERS
    src/Core/MeshData.hpp
    src/Core/TextureData.hpp
)

set(IO_SOURCES
    src/IO/MeshLoader.cpp
)

set(IO_HEADERS
    src/IO/MeshLoader.hpp
)

set(COMMANDS_HEADERS
    src/Commands/CommandBase.hpp
)

# Combine all sources
set(ALL_SOURCES
    ${MAIN_SOURCES}
    ${RENDER_SOURCES}
    ${CORE_SOURCES}
    ${IO_SOURCES}
)

set(ALL_HEADERS
    ${MAIN_HEADERS}
    ${RENDER_HEADERS}
    ${CORE_HEADERS}
    ${IO_HEADERS}
    ${COMMANDS_HEADERS}
)

# =============================================================================
# Create executable
# =============================================================================
add_executable(MoldWing WIN32
    ${ALL_SOURCES}
    ${ALL_HEADERS}
)

# Enable Qt MOC/UIC/RCC
set_target_properties(MoldWing PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
    # Disable vcpkg applocal DLL copying - we handle Qt deployment manually
    VS_GLOBAL_VcpkgEnabled false
)

# =============================================================================
# Include directories
# =============================================================================
set(DILIGENT_CORE_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/DiligentEngine/DiligentCore")

target_include_directories(MoldWing PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}
    # DiligentEngine includes
    ${DILIGENT_CORE_DIR}
    ${DILIGENT_CORE_DIR}/Graphics/GraphicsEngine/interface
    ${DILIGENT_CORE_DIR}/Graphics/GraphicsEngineD3D11/interface
    ${DILIGENT_CORE_DIR}/Graphics/GraphicsEngineD3D12/interface
    ${DILIGENT_CORE_DIR}/Graphics/GraphicsTools/interface
    ${DILIGENT_CORE_DIR}/Common/interface
    ${DILIGENT_CORE_DIR}/Primitives/interface
    ${DILIGENT_CORE_DIR}/Platforms/interface
    ${DILIGENT_CORE_DIR}/Platforms/Basic/interface
    ${DILIGENT_CORE_DIR}/Platforms/Win32/interface
)

# =============================================================================
# Link libraries
# =============================================================================
target_link_libraries(MoldWing PRIVATE
    # Qt6
    Qt6::Widgets
    Qt6::Gui

    # DiligentEngine
    Diligent-GraphicsEngineD3D11-shared
    Diligent-GraphicsEngineD3D12-shared
    Diligent-Common

    # Model loading
    assimp::assimp
)

# =============================================================================
# Compile definitions
# =============================================================================
target_compile_definitions(MoldWing PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    D3D11_SUPPORTED
    D3D12_SUPPORTED
    ENGINE_DLL
    PLATFORM_WIN32
)

# =============================================================================
# Platform-specific settings
# =============================================================================
if(MSVC)
    target_compile_options(MoldWing PRIVATE
        /W4
        /wd4201
        /wd4819
        /MP
    )

    set_target_properties(MoldWing PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

# =============================================================================
# Copy DiligentEngine DLLs
# =============================================================================
if(WIN32)
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Diligent-GraphicsEngineD3D11-shared>
            $<TARGET_FILE:Diligent-GraphicsEngineD3D12-shared>
            $<TARGET_FILE_DIR:MoldWing>
        COMMENT "Copying DiligentEngine DLLs..."
    )
endif()

# =============================================================================
# Deploy Qt (copy DLLs and plugins)
# =============================================================================
if(WIN32)
    # vcpkg Qt paths
    set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")

    # Copy Qt DLLs (debug goes to debug/bin, release goes to bin)
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_INSTALLED_DIR}/$<$<CONFIG:Debug>:debug/>bin/Qt6Core$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_INSTALLED_DIR}/$<$<CONFIG:Debug>:debug/>bin/Qt6Gui$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_INSTALLED_DIR}/$<$<CONFIG:Debug>:debug/>bin/Qt6Widgets$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>"
        COMMENT "Copying Qt DLLs..."
    )

    # Create platforms directory and copy qwindows plugin
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MoldWing>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_INSTALLED_DIR}/$<$<CONFIG:Debug>:debug/>Qt6/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>/platforms/"
        COMMENT "Copying Qt platform plugin..."
    )

    # Create styles directory and copy style plugin (optional, may not exist)
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MoldWing>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_INSTALLED_DIR}/$<$<CONFIG:Debug>:debug/>Qt6/plugins/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>/styles/"
        COMMENT "Copying Qt style plugin..."
        COMMAND_EXPAND_LISTS
    )
endif()

# =============================================================================
# IDE folder organization
# =============================================================================
set_target_properties(MoldWing PROPERTIES FOLDER "MoldWing/Apps")

source_group("src" FILES ${MAIN_SOURCES} ${MAIN_HEADERS})
source_group("src/Render" FILES ${RENDER_SOURCES} ${RENDER_HEADERS})
source_group("src/Core" FILES ${CORE_SOURCES} ${CORE_HEADERS})
source_group("src/IO" FILES ${IO_SOURCES} ${IO_HEADERS})
source_group("src/Commands" FILES ${COMMANDS_HEADERS})
