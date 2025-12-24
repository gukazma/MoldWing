set(TARGET_NAME "VulkanEngine")
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
get_filename_component(NAMESPACE ${PARENT_DIR} NAME)

option(${NAMESPACE}_${TARGET_NAME}_SHARED "target shared lib" ON)

configure_file(Export.h.in ${CMAKE_CURRENT_LIST_DIR}/Export.h @ONLY)
file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE HPP_FILES "*.hpp")
file(GLOB_RECURSE CPP_FILES "*.cpp")
file(GLOB_RECURSE CC_FILES "*.cc")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${HPP_FILES} ${CPP_FILES} ${CC_FILES})

if(${NAMESPACE}_${TARGET_NAME}_SHARED)
add_library(${TARGET_NAME} SHARED ${HEADER_FILES} ${HPP_FILES} ${CPP_FILES} ${CC_FILES})
target_compile_definitions(${TARGET_NAME} PUBLIC ${TARGET_NAME}_DLL_LIB)
target_compile_definitions(${TARGET_NAME} PRIVATE ${TARGET_NAME}_DLL_LIB_EXPORT)
else()
add_library(${TARGET_NAME} STATIC ${HEADER_FILES} ${HPP_FILES} ${CPP_FILES} ${CC_FILES})
endif()

add_library(${NAMESPACE}::${TARGET_NAME} ALIAS ${TARGET_NAME})

set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER Modules/${NAMESPACE})

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")
target_compile_options(${TARGET_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Find Vulkan
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenCV REQUIRED)

# Link dependencies
target_link_libraries(${TARGET_NAME} PUBLIC
    Vulkan::Vulkan
    glfw
    opencv_core
    opencv_imgcodecs
    opencv_imgproc
)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>
)

install(TARGETS ${TARGET_NAME}
  EXPORT ${TARGET_NAME}
  INCLUDES DESTINATION include/${NAMESPACE}
)

install(FILES ${HEADER_FILES} DESTINATION include/${NAMESPACE}/${TARGET_NAME})
install(FILES ${HPP_FILES} DESTINATION include/${NAMESPACE}/${TARGET_NAME})

install(EXPORT ${TARGET_NAME}
  DESTINATION cmake/${TARGET_NAME}
  FILE ${TARGET_NAME}-config.cmake
  NAMESPACE ${NAMESPACE}::
)
