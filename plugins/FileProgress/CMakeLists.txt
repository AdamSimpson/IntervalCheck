project(LibFileProgress)
cmake_minimum_required(VERSION 3.1)

find_package(CUDA REQUIRED)

# Add Titan specific includes
include_directories("/sw/xk6/nvml/include" "/opt/cray/alps/default/include")

# Add Titan specific libraries
link_directories("/opt/cray/alps/default/lib64/")

# Shared GPUhealthTitan library
add_library(FileProgress SHARED src/File_Progress.c)
set_target_properties(FileProgress PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
set_property(TARGET FileProgress PROPERTY C_STANDARD 99)

target_link_libraries(FileProgress libalpslli.so)

# Hack as the PIC option for set_target_properies doesn't appear to work for CCE
if(CMAKE_C_COMPILER_ID MATCHES "Cray")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

install(TARGETS FileProgress DESTINATION lib)
