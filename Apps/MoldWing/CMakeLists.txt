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
# Find spdlog
# =============================================================================
find_package(spdlog CONFIG REQUIRED)

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
    src/Render/PivotIndicator.cpp
    src/Render/SelectionBoxRenderer.cpp
    src/Render/BrushCursorRenderer.cpp
)

set(RENDER_HEADERS
    src/Render/DiligentWidget.hpp
    src/Render/CameraSettings.hpp
    src/Render/OrbitCamera.hpp
    src/Render/MeshRenderer.hpp
    src/Render/PivotIndicator.hpp
    src/Render/SelectionBoxRenderer.hpp
    src/Render/BrushCursorRenderer.hpp
)

set(CORE_SOURCES
    src/Core/MeshData.cpp
    src/Core/Logger.cpp
)

set(CORE_HEADERS
    src/Core/MeshData.hpp
    src/Core/TextureData.hpp
    src/Core/Logger.hpp
    src/Core/RayIntersection.hpp
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

set(SELECTION_SOURCES
    src/Selection/FacePicker.cpp
    src/Selection/SelectionSystem.cpp
    src/Selection/SelectionRenderer.cpp
)

set(SELECTION_HEADERS
    src/Selection/FacePicker.hpp
    src/Selection/SelectionSystem.hpp
    src/Selection/SelectionRenderer.hpp
)

# Combine all sources
set(ALL_SOURCES
    ${MAIN_SOURCES}
    ${RENDER_SOURCES}
    ${CORE_SOURCES}
    ${IO_SOURCES}
    ${SELECTION_SOURCES}
)

set(ALL_HEADERS
    ${MAIN_HEADERS}
    ${RENDER_HEADERS}
    ${CORE_HEADERS}
    ${IO_HEADERS}
    ${COMMANDS_HEADERS}
    ${SELECTION_HEADERS}
)

# =============================================================================
# Create executable (remove WIN32 for debug console output)
# =============================================================================
add_executable(MoldWing
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

    # Logging
    spdlog::spdlog
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
    # spdlog: 启用 TRACE 级别日志
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
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
# Deploy Qt and dependencies (copy DLLs and plugins)
# =============================================================================
if(WIN32)
    # vcpkg paths
    set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
    set(VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/$<$<CONFIG:Debug>:debug/>bin")
    set(VCPKG_PLUGINS_DIR "${VCPKG_INSTALLED_DIR}/$<$<CONFIG:Debug>:debug/>Qt6/plugins")

    # Copy Qt core DLLs
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_BIN_DIR}/Qt6Core$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/Qt6Gui$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/Qt6Widgets$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>"
        COMMENT "Copying Qt DLLs..."
    )

    # Copy Qt dependency DLLs
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_BIN_DIR}/freetype$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/pcre2-16$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/zlib$<$<CONFIG:Debug>:d>1.dll"
            "${VCPKG_BIN_DIR}/bz2$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/libpng16$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/brotlicommon.dll"
            "${VCPKG_BIN_DIR}/brotlidec.dll"
            "${VCPKG_BIN_DIR}/double-conversion.dll"
            "$<TARGET_FILE_DIR:MoldWing>"
        COMMENT "Copying Qt dependency DLLs..."
    )

    # Copy assimp and its dependencies
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_BIN_DIR}/assimp-vc143-mt$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/minizip.dll"
            "${VCPKG_BIN_DIR}/poly2tri.dll"
            "${VCPKG_BIN_DIR}/pugixml.dll"
            "$<TARGET_FILE_DIR:MoldWing>"
        COMMENT "Copying assimp DLLs..."
    )

    # Copy spdlog and fmt DLLs
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_BIN_DIR}/spdlog$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_BIN_DIR}/fmt$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>"
        COMMENT "Copying spdlog DLLs..."
    )

    # Create platforms directory and copy qwindows plugin
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MoldWing>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_PLUGINS_DIR}/platforms/qwindows$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>/platforms/"
        COMMENT "Copying Qt platform plugin..."
    )

    # Create styles directory and copy style plugin
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MoldWing>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_PLUGINS_DIR}/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>/styles/"
        COMMENT "Copying Qt style plugin..."
    )

    # Create imageformats directory and copy plugins
    add_custom_command(TARGET MoldWing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MoldWing>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_PLUGINS_DIR}/imageformats/qgif$<$<CONFIG:Debug>:d>.dll"
            "${VCPKG_PLUGINS_DIR}/imageformats/qico$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:MoldWing>/imageformats/"
        COMMENT "Copying Qt imageformats plugins..."
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
source_group("src/Selection" FILES ${SELECTION_SOURCES} ${SELECTION_HEADERS})
