set(TARGET_NAME Mesh)

file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADERS} ${SOURCES})

add_library(${TARGET_NAME} SHARED)

target_sources(${TARGET_NAME}
    PRIVATE
        ${HEADERS}
        ${SOURCES}
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)
set_target_properties(${TARGET_NAME} PROPERTIES CXX_EXTENSIONS OFF)

# Find required packages
find_package(CGAL REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        CGAL::CGAL
        tinyobjloader::tinyobjloader
)

# Export debug/release suffix
set_target_properties(${TARGET_NAME} PROPERTIES DEBUG_POSTFIX "d")

# Define export macros
target_compile_definitions(${TARGET_NAME}
    PRIVATE
        MESH_EXPORTS
    PUBLIC
        $<$<CONFIG:Debug>:MESH_DEBUG>
)

# MSVC specific settings
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Create alias
add_library(MoldWing::Mesh ALIAS ${TARGET_NAME})

install(TARGETS ${TARGET_NAME}
    EXPORT MeshTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)
