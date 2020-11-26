cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME fim)
project(${PROJECT_NAME} VERSION 0.2.0 LANGUAGES CXX)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
	set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Build Mode: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PythonLibs COMPONENTS Development REQUIRED)
find_package(OpenMP)

add_definitions(-DMODULE_NAME=${PROJECT_NAME} -DWITH_SIG_TERM)

include_directories(
    include
		${PYTHON_INCLUDE_DIRS}
)

file(GLOB src "src/*.cpp")

add_library(${PROJECT_NAME} SHARED ${src})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON) # enable fPIC
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "") # disable the "lib" prefix

if (OPENMP_FOUND)
	target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME}} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -pedantic -Wextra -Weffc++ -Wunused-result -Werror)
endif()