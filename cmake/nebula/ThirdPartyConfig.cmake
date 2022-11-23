message(">>>> Configuring third party for '${PROJECT_NAME}' <<<<")
# The precedence to decide NEBULA_THIRDPARTY_ROOT is:
#   1. The path defined with CMake argument, i.e -DNEBULA_THIRDPARTY_ROOT=path
#   2. ${CMAKE_BINARY_DIR}/third-party/install, if exists
#   3. The path specified with environment variable NEBULA_THIRDPARTY_ROOT=path
#   4. /opt/vesoft/third-party, if exists
#   5. At last, one copy will be downloaded and installed to ${CMAKE_BINARY_DIR}/third-party/install

set(NEBULA_THIRDPARTY_VERSION "2.0")

if(${DISABLE_CXX11_ABI})
    SET(NEBULA_THIRDPARTY_ROOT ${CMAKE_BINARY_DIR}/third-party-98/install)
    if(NOT EXISTS ${CMAKE_BINARY_DIR}/third-party-98/install)
        message(STATUS "Install abi 98 third-party")
        include(InstallThirdParty)
    endif()
else()
    if("${NEBULA_THIRDPARTY_ROOT}" STREQUAL "")
        if(EXISTS ${CMAKE_BINARY_DIR}/third-party/install)
            SET(NEBULA_THIRDPARTY_ROOT ${CMAKE_BINARY_DIR}/third-party/install)
        elseif(NOT $ENV{NEBULA_THIRDPARTY_ROOT} STREQUAL "")
            SET(NEBULA_THIRDPARTY_ROOT $ENV{NEBULA_THIRDPARTY_ROOT})
        elseif(EXISTS /opt/vesoft/third-party/${NEBULA_THIRDPARTY_VERSION})
            SET(NEBULA_THIRDPARTY_ROOT "/opt/vesoft/third-party/${NEBULA_THIRDPARTY_VERSION}")
        else()
            include(InstallThirdParty)
        endif()
    endif()
endif()

if(NOT ${NEBULA_THIRDPARTY_ROOT} STREQUAL "")
    print_config(NEBULA_THIRDPARTY_ROOT)
    file(READ ${NEBULA_THIRDPARTY_ROOT}/version-info third_party_build_info)
    message(STATUS "Build info of nebula third party:\n${third_party_build_info}")
    list(INSERT CMAKE_INCLUDE_PATH 0 ${NEBULA_THIRDPARTY_ROOT}/include)
    list(INSERT CMAKE_LIBRARY_PATH 0 ${NEBULA_THIRDPARTY_ROOT}/lib)
    list(INSERT CMAKE_LIBRARY_PATH 0 ${NEBULA_THIRDPARTY_ROOT}/lib64)
    list(INSERT CMAKE_PROGRAM_PATH 0 ${NEBULA_THIRDPARTY_ROOT}/bin)
    include_directories(SYSTEM ${NEBULA_THIRDPARTY_ROOT}/include)
    link_directories(
        ${NEBULA_THIRDPARTY_ROOT}/lib
        ${NEBULA_THIRDPARTY_ROOT}/lib64
    )
endif()

if(NOT ${NEBULA_OTHER_ROOT} STREQUAL "")
    string(REPLACE ":" ";" DIR_LIST ${NEBULA_OTHER_ROOT})
    list(LENGTH DIR_LIST len)
    foreach(DIR IN LISTS DIR_LIST )
        list(INSERT CMAKE_INCLUDE_PATH 0 ${DIR}/include)
        list(INSERT CMAKE_LIBRARY_PATH 0 ${DIR}/lib)
        list(INSERT CMAKE_PROGRAM_PATH 0 ${DIR}/bin)
        include_directories(SYSTEM ${DIR}/include)
        link_directories(${DIR}/lib)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L ${DIR}/lib")
    endforeach()
endif()

print_config(CMAKE_INCLUDE_PATH)
print_config(CMAKE_LIBRARY_PATH)
print_config(CMAKE_PROGRAM_PATH)

execute_process(
    COMMAND ldd --version
    COMMAND head -1
    COMMAND cut -d ")" -f 2
    COMMAND cut -d " " -f 2
    OUTPUT_VARIABLE GLIBC_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
print_config(GLIBC_VERSION)

if (GLIBC_VERSION VERSION_LESS "2.17")
    set(GETTIME_LIB rt)
else()
    set(GETTIME_LIB)
endif()

message("")


find_package(folly CONFIG REQUIRED)
set(folly_LIBRARIES Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util)

find_package(gtest CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
set(glog_LIBRARIES glog::glog)

find_package(Boost REQUIRED)
find_package(Sodium REQUIRED)

# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L ${NEBULA_THIRDPARTY_ROOT}/lib")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L ${NEBULA_THIRDPARTY_ROOT}/lib64")


# All thrift libraries
find_package(glog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Sodium REQUIRED)
find_package(wangle CONFIG REQUIRED)
find_package(FBThrift CONFIG REQUIRED)
find_package(fizz CONFIG REQUIRED)
find_package(folly CONFIG REQUIRED)
set(FBThrift_LIBRARIES
    fizz::fizz
    ${Sodium_LIBRARY}
    wangle::wangle
    FBThrift::compiler_ast
    FBThrift::compiler_base
    FBThrift::compiler_lib
    FBThrift::mustache_lib
    FBThrift::thrift-core
    FBThrift::concurrency
    FBThrift::transport
    FBThrift::async
    FBThrift::thrift
    FBThrift::rpcmetadata
    FBThrift::thriftmetadata
    FBThrift::thriftfrozen2
    FBThrift::thrifttype
    FBThrift::thriftprotocol
    FBThrift::thriftcpp2
    Folly::folly
)

message(">>>> Configuring third party for '${PROJECT_NAME}' done <<<<")
