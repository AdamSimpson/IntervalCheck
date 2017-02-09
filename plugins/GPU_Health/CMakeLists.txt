project(LibGPUhealth)
cmake_minimum_required(VERSION 3.1)

find_package(CUDA REQUIRED)

include_directories(${CUDA_INCLUDE_DIRS})

# Try to add cuda driver libraries
link_directories("/usr/lib/nvidia")

# Shared GPUhealth library
add_library(GPUhealth SHARED src/GPU_Health.c)
set_target_properties(GPUhealth PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(GPUhealth libnvidia-ml.so)
target_link_libraries(GPUhealth ${CUDA_LIBRARIES})

# Hack as the PIC option for set_target_properies doesn't appear to work for CCE
if(CMAKE_C_COMPILER_ID MATCHES "Cray")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

install(TARGETS GPUhealth DESTINATION lib)
