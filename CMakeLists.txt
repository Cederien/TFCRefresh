cmake_minimum_required(VERSION 3.25)

project(
	TFCRefresh
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/CommonLibSSE CommonLibSSE EXCLUDE_FROM_ALL)
include(extern/CommonLibSSE/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(
	${PROJECT_NAME}
	AUTHOR "Cederien"
	SOURCES src/main.cpp
)

target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
		src/PCH.h
)

target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /permissive- /utf-8 /W4)
