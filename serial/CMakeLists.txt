cmake_minimum_required(VERSION 3.10)
project(cs2050_serial VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release for performance builds; override with -DCMAKE_BUILD_TYPE=Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Collect all C/C++ sources placed in serial/src/
file(GLOB SERIAL_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

if(NOT SERIAL_SOURCES)
  message(WARNING "No source files found in ${CMAKE_CURRENT_SOURCE_DIR}/src. Add .cpp/.c files.")
endif()

add_executable(serial_exec ${SERIAL_SOURCES})
target_include_directories(serial_exec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Optimization flags for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(serial_exec PRIVATE -O3 -march=native)
endif()

install(TARGETS serial_exec DESTINATION bin)

# Usage notes:
# - Place source files in serial/src/ (e.g. main.cpp, neighbor_list.cpp).
# - Run cmake from the `serial` directory or configure from the top-level with an appropriate build directory.