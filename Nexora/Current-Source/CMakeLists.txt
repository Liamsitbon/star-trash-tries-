cmake_minimum_required(VERSION 3.22)

option(QUEST "Build for Quest" ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_compile_options(
  -frtti -fexceptions -fvisibility=hidden -fPIE -fPIC
  -ffunction-sections -fdata-sections
  -Wall -Wextra -Wno-unused-parameter
)

include(${CMAKE_CURRENT_SOURCE_DIR}/qpm_defines.cmake)

if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
  add_compile_options(-O3 -flto)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
  add_compile_options(-g)
endif()

project(Nexora VERSION ${MOD_VERSION})
add_compile_definitions(MOD_ID="nexora" MOD_NAME="Nexora" VERSION="${CMAKE_PROJECT_VERSION}")
set(COMPILE_ID ${CMAKE_PROJECT_NAME})

RECURSE_FILES(cpp_file_list ${SOURCE_DIR}/*.cpp)
RECURSE_FILES(c_file_list ${SOURCE_DIR}/*.c)
add_library(${CMAKE_PROJECT_NAME} SHARED ${cpp_file_list} ${c_file_list})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE -llog)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,--gc-sections)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${INCLUDE_DIR} ${SHARED_DIR})

RECURSE_FILES(src_inline_hook_c ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.c)
RECURSE_FILES(src_inline_hook_cpp ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.cpp)
target_sources(${COMPILE_ID} PRIVATE ${src_inline_hook_c} ${src_inline_hook_cpp})

if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/debug"
    COMMAND ${CMAKE_COMMAND} -E copy
      "${CMAKE_CURRENT_BINARY_DIR}/lib${CMAKE_PROJECT_NAME}.so"
      "${CMAKE_CURRENT_BINARY_DIR}/debug/lib${CMAKE_PROJECT_NAME}.so"
    COMMAND ${CMAKE_STRIP} --strip-all "${CMAKE_CURRENT_BINARY_DIR}/lib${CMAKE_PROJECT_NAME}.so"
    COMMENT "Preserving Nexora debug symbols and stripping the packaged runtime"
  )
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/extern.cmake)

