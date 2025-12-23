set(TARGET_NAME 04_CameraControl)

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
)

target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Compile cube shaders to C++ headers (reuse from 02_ImGuiBasic)
compile_shaders_to_headers(
    TARGET ${TARGET_NAME}
    SOURCES
        ${CMAKE_SOURCE_DIR}/Apps/02_ImGuiBasic/shaders/cube.vert
        ${CMAKE_SOURCE_DIR}/Apps/02_ImGuiBasic/shaders/cube.frag
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders
    OPTIMIZE
)

install(TARGETS ${TARGET_NAME})
