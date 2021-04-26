cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME fim)
project(${PROJECT_NAME} VERSION 0.2.0 LANGUAGES CXX)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
	set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Build Mode: ${CMAKE_BUILD_TYPE}")

option(USE_OPENMP "Build the project with OpenMP support" ON)
option(USE_MPI "Build the project with MPI support" OFF)
option(EVAL_MODE "Build the project in eval mode, disabling most printing" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
  if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  else()
	# Windows-GCC library for memory utilization
	find_library(PSAPI Psapi)
  endif()
endif()

if (USE_OPENMP)
  find_package(OpenMP REQUIRED)
  add_definitions(-DUSE_OPENMP)
endif()

include_directories(include)

if (USE_MPI)
  find_package(MPI REQUIRED)
  add_definitions(-DUSE_MPI)
  include_directories(SYSTEM ${MPI_INCLUDE_PATH})
endif()

if (EVAL_MODE)
  add_definitions(-DEVAL_MODE)
  message(STATUS "Eval Mode enabled")
endif()

find_package(PythonLibs COMPONENTS Development REQUIRED)
add_definitions(-DMODULE_NAME=${PROJECT_NAME} -DWITH_SIG_TERM)
include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})

file(GLOB src "src/*.cpp")
add_library(${PROJECT_NAME} SHARED ${src})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "") # disable the "lib" prefix

if (WIN32)
  # Change the extension from .dll to .pyd on Windows
  set(CMAKE_SHARED_LIBRARY_SUFFIX ".pyd")
  target_link_libraries(${PROJECT_NAME} PUBLIC ${PSAPI})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PYTHON_LIBRARIES})
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON) # enable fPIC

if (OPENMP_FOUND)
	target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

if (MPI_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${MPI_CXX_LIBRARIES})
endif()


if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -pedantic -Wextra -Weffc++ -Wunused-result -Werror)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/../Evaluation
)
