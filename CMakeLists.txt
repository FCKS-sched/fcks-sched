cmake_minimum_required(VERSION 3.18)
project(fcks_sched)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_subdirectory(third_party/xgboost)

# sched_ext
set(SCX_DIR "${CMAKE_SOURCE_DIR}/third_party/scx")
set(SCX_BIN_OUT "${CMAKE_BINARY_DIR}/schedulers")

set(SCX_RUST_TARGETS
    scx_bpfland
    scx_lavd
    scx_layered
    scx_rusty
    scx_p2dq
    scx_flash
    scx_eevdf
    scx_pandemonium
)

set(CARGO_PKG_FLAGS "")
foreach(SCHED ${SCX_RUST_TARGETS})
    list(APPEND CARGO_PKG_FLAGS -p ${SCHED})
endforeach()

add_custom_target(build_scx_schedulers ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SCX_BIN_OUT}"
    COMMAND cargo build --release ${CARGO_PKG_FLAGS}
    WORKING_DIRECTORY "${SCX_DIR}"
    COMMENT "Building selected sched_ext schedulers..."
)

foreach(SCHED ${SCX_RUST_TARGETS})
    add_custom_command(TARGET build_scx_schedulers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SCX_DIR}/target/release/${SCHED}"
            "${SCX_BIN_OUT}/${SCHED}"
    )
endforeach()

# ZMQPP
find_library(ZMQ_SHARED_LIB NAMES zmq REQUIRED)
set(ZEROMQ_LIBRARY_STATIC "${ZMQ_SHARED_LIB}" CACHE FILEPATH "" FORCE)

set(ZMQPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZMQPP_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(ZMQPP_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZMQPP_BUILD_SHARED ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    zmqpp
    GIT_REPOSITORY https://github.com/zeromq/zmqpp.git
    GIT_TAG        4.2.0
)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(zmqpp)
target_include_directories(zmqpp-static PUBLIC "${zmqpp_SOURCE_DIR}/src")

add_executable(fcks_sched
    src/main.cpp
    src/policy_manager.cpp
)
target_link_libraries(fcks_sched PRIVATE xgboost zmqpp-static)
add_dependencies(fcks_sched build_scx_schedulers)