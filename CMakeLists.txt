
cmake_minimum_required(VERSION 2.6)
project(flFTP LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "-Wall -g -O2")
option(DFL_BUILD_SHARED "Build shared library" OFF)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)


add_subdirectory(${PROJECT_SOURCE_DIR}/tinyxml2)
link_directories(${PROJECT_SOURCE_DIR}/tinyxml2)

if(DFL_BUILD_SHARED)
	add_library(flFTP SHARED flFTP.cpp flFTP.h)
else()
	add_library(flFTP STATIC flFTP.cpp flFTP.h)
endif()

add_executable(flTest flFTP.cpp test.cpp flFTP.h)
add_dependencies(flTest flFTP)


install(TARGETS flFTP 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES flFTP.h DESTINATION include)


if(UNIX)
	target_link_libraries(flFTP pthread)
	target_link_libraries(flTest pthread)
endif()

target_link_libraries(flFTP tinyxml2)
target_link_libraries(flTest tinyxml2)
