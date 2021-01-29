cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME fim)
project(${PROJECT_NAME} VERSION 0.2.0 LANGUAGES CXX)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
	set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Build Mode: ${CMAKE_BUILD_TYPE}")

option(USE_OPENMP "Build the project with OpenMP support" ON)
option(USE_MPI "Build the project with MPI support" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (USE_OPENMP)
  add_definitions(-DUSE_OPENMP)
  find_package(OpenMP REQUIRED)
endif()

include_directories(include)

# Currently MPI and Python are mutually exclusive
if (USE_MPI)
  find_package(MPI REQUIRED)
  add_definitions(-DUSE_MPI)
  include_directories(SYSTEM ${MPI_INCLUDE_PATH})

  file(GLOB src "main.cpp")
  add_executable(${PROJECT_NAME} ${src})
else()
  find_package(PythonLibs COMPONENTS Development REQUIRED)
  add_definitions(-DMODULE_NAME=${PROJECT_NAME} -DWITH_SIG_TERM)
  include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})

  file(GLOB src "src/*.cpp")
  add_library(${PROJECT_NAME} SHARED ${src})
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "") # disable the "lib" prefix
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON) # enable fPIC

if (OPENMP_FOUND)
	target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${MPI_CXX_LIBRARIES})

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -pedantic -Wextra -Weffc++ -Wunused-result -Werror)
endif()
