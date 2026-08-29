cmake_minimum_required(VERSION 3.22)

option(QUEST "Build for quest" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_compile_options(
	-frtti
	-fexceptions
	-fvisibility=hidden
	-fPIE
	-fPIC
	-ffunction-sections
	-fdata-sections
)

# QPM defines - sets up NDK, RECURSE_FILES macro, EXTERN_DIR, SHARED_DIR
include(${CMAKE_CURRENT_SOURCE_DIR}/qpm_defines.cmake)

if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
	add_compile_options(-O3)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	add_compile_options(-flto)
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
	add_compile_options(-g)
endif()

project(Vivify
	VERSION ${MOD_VERSION})

add_compile_definitions(MOD_ID="${CMAKE_PROJECT_NAME}")
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

set(COMPILE_ID ${CMAKE_PROJECT_NAME})

# Recursively get all src files
RECURSE_FILES(cpp_file_list ${SOURCE_DIR}/*.cpp)
RECURSE_FILES(c_file_list ${SOURCE_DIR}/*.c)

add_library(
	${CMAKE_PROJECT_NAME}
	SHARED
	${cpp_file_list}
	${c_file_list}
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE -llog)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,--gc-sections)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${INCLUDE_DIR})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${SHARED_DIR})

# beatsaber hook inline hook
RECURSE_FILES(src_inline_hook_beatsaber_hook_local_extra_c ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.c)
RECURSE_FILES(src_inline_hook_beatsaber_hook_local_extra_cpp ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.cpp)

target_sources(${COMPILE_ID} PRIVATE ${src_inline_hook_beatsaber_hook_local_extra_c})
target_sources(${COMPILE_ID} PRIVATE ${src_inline_hook_beatsaber_hook_local_extra_cpp})

# Keep an unstripped diagnostics binary while packaging an optimized runtime binary.
if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
	add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/debug"
		COMMAND ${CMAKE_COMMAND} -E copy
			"$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
			"$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/debug/lib${CMAKE_PROJECT_NAME}.so"
		COMMAND ${CMAKE_STRIP} --strip-all
			"$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
		COMMENT "Preserving debug symbols and stripping the packaged Vivify library"
	)
endif()

# QPM extern includes and libs
include(${CMAKE_CURRENT_SOURCE_DIR}/extern.cmake)
