project(LibIntervalCheck)
cmake_minimum_required(VERSION 3.1)

# Shared IntervalCheck library
add_library(IntervalCheck SHARED src/IntervalCheck.c)
set_target_properties(IntervalCheck PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

set_property(TARGET IntervalCheck PROPERTY C_STANDARD 99)

# Hack as the PIC option for set_target_properies doesn't appear to work for CCE
if(CMAKE_C_COMPILER_ID MATCHES "Cray")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

install(TARGETS IntervalCheck DESTINATION lib)
