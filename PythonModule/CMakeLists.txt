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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(COMPILER_NAME "Clang")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(WIN32)
		set(COMPILER_NAME "MinGW")
	else()
		set(COMPILER_NAME "GCC")
	endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	set(COMPILER_NAME "Intel C++")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(COMPILER_NAME "Microsoft Visual Studio")
else()
	set(COMPILER_NAME CMAKE_CXX_COMPILER_ID)
endif()

add_definitions(-DCOMPILER_STR="${COMPILER_NAME}")


# GCC flag to enable large address awareness, used to increase the amount of memory
# usable by a 32-bit application
set(LAA_FLAG "-Wl,--large-address-aware")

if (WIN32)
  if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_UNICODE)
	add_definitions(-DUNICODE)
	set(CMAKE_CXX_FLAGS "/EHsc")
	set(LAA_FLAG "/LARGEADDRESSAWARE")
  else()
	# Windows-GCC library for memory utilization
	# find_library(PSAPI Psapi)
	set(PSAPI Psapi)
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

# Set large address aware flag for 32-bit, this allows for up to 3GB of memory to be used
# However, in most cases Python is not build in large address aware mode, meaning that once
# 2GB of memory are used the application will terminate itself
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	add_definitions(-DARCH_X86)
	message(WARNING "Building a 32-bit application is discouraged as the maximum amount of usable memory is limited to 2GB")
	target_link_options(${PROJECT_NAME} PRIVATE ${LAA_FLAG})
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -pedantic -Wextra -Weffc++ -Wunused-result -Werror)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/../Evaluation
)
