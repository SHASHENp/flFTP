
cmake_minimum_required(VERSION 3.2)
project(flFTP LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "-Wall -g -O2")
option(DFL_BUILD_SHARED "Build shared library" OFF)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

set(DFL_SOURCE_FILES "tinyxml2/tinyxml2.cpp" "flFTP.cpp")
set(DFL_HEADER_FILES "tinyxml2/tinyxml2.h" "flFTP.h")


if(DFL_BUILD_SHARED)
	add_library(flFTP SHARED ${DFL_SOURCE_FILES} ${DFL_HEADER_FILES})
else()
	add_library(flFTP STATIC ${DFL_SOURCE_FILES} ${DFL_HEADER_FILES})
endif()


target_include_directories(flFTP PRIVATE 
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/tinyxml2>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")


add_executable(flTest test.cpp)
add_dependencies(flTest flFTP)


install(TARGETS flFTP 
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib)

install(FILES flFTP.h DESTINATION include)


if(UNIX)
	target_link_libraries(flFTP PUBLIC pthread)
endif()

target_link_libraries(flTest flFTP)
