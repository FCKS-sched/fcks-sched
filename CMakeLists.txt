cmake_minimum_required(VERSION 3.18)
project(fcks_sched)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_subdirectory(xgboost)

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

add_executable(fcks_sched src/main.cpp)
target_link_libraries(fcks_sched PRIVATE xgboost zmqpp-static)