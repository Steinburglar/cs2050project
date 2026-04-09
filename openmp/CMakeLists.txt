cmake_minimum_required(VERSION 3.10)
project(cs2050_openmp VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenMP REQUIRED)

file(GLOB OPENMP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(openmp_exec ${OPENMP_SOURCES})
target_include_directories(openmp_exec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(openmp_exec PUBLIC OpenMP::OpenMP_CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(openmp_exec PRIVATE -O3 -march=native)
endif()

install(TARGETS openmp_exec DESTINATION bin)
