cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/qpm_defines.cmake)
project(NEFixed VERSION 0.1.0)
set(COMPILE_ID NEFixed)
add_library(NEFixed SHARED src/main.cpp src/NoteCompatibility.cpp src/CutoutPrecision.cpp)
target_compile_definitions(NEFixed PRIVATE MOD_ID="ne-fixed" VERSION="${MOD_VERSION}")
target_compile_options(NEFixed PRIVATE
  -frtti -fexceptions -fvisibility=hidden -fPIC
  -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter
  -Werror=return-type)
target_include_directories(NEFixed PRIVATE include)
target_link_options(NEFixed PRIVATE -Wl,--gc-sections -Wl,--no-undefined)
target_link_libraries(NEFixed PRIVATE log)
include(${CMAKE_CURRENT_SOURCE_DIR}/extern.cmake)
RECURSE_FILES(inline_hook_c ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.c)
RECURSE_FILES(inline_hook_cpp ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.cpp)
target_sources(NEFixed PRIVATE ${inline_hook_c} ${inline_hook_cpp})
if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
  add_custom_command(TARGET NEFixed POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/debug"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:NEFixed>" "${CMAKE_CURRENT_BINARY_DIR}/debug/libNEFixed.so"
    COMMAND ${CMAKE_STRIP} --strip-all "$<TARGET_FILE:NEFixed>")
endif()
