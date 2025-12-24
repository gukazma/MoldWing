set(TARGET_NAME 05_ModelDemo)

file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADERS} ${SOURCES})

add_executable(${TARGET_NAME})

target_sources(${TARGET_NAME}
    PRIVATE
        ${HEADERS}
        ${SOURCES}
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)
set_target_properties(${TARGET_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER Apps)

# Find required packages
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
        MoldWing::VulkanEngine
        MoldWing::Mesh
        MoldWing::Shaders
)

target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Copy assets to build directory
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${TARGET_NAME}>/assets
    COMMENT "Copying assets to build directory"
)

install(TARGETS ${TARGET_NAME})

# Install assets
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
        DESTINATION bin/assets
        FILES_MATCHING
        PATTERN "*.obj"
        PATTERN "*.png"
        PATTERN "*.py" EXCLUDE)
