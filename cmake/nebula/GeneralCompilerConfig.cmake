set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

print_config(CMAKE_CXX_STANDARD)
print_config(CMAKE_CXX_COMPILER)
print_config(CMAKE_CXX_COMPILER_ID)

if (!CMAKE_CXX_COMPILER)
    message(FATAL_ERROR "No C++ compiler found")
endif()

include(CheckCXXCompilerFlag)

# For s2
add_definitions(-DS2_USE_GLOG)
add_definitions(-DS2_USE_GFLAGS)

set(CMAKE_POSITION_INDEPENDENT_CODE ${ENABLE_PIC})

if(ENABLE_TESTING AND ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(--coverage)
    add_compile_options(-g)
    add_compile_options(-O0)
    nebula_add_exe_linker_flag(-coverage)
    nebula_add_exe_linker_flag(-lgcov)
endif()