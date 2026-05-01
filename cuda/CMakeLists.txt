cmake_minimum_required(VERSION 3.18)
project(cs2050_cuda VERSION 1.0 LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

file(GLOB CUDA_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cu"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(cuda_exec ${CUDA_SOURCES})
target_include_directories(cuda_exec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(cuda_exec PROPERTIES CUDA_SEPARABLE_COMPILATION ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cuda_exec PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-O3 -march=native>
    $<$<COMPILE_LANGUAGE:CUDA>:-O3>
  )
endif()

install(TARGETS cuda_exec DESTINATION bin)
