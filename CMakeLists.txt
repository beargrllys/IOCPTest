cmake_minimum_required(VERSION 3.16)
project(IOCPTest)

set(BOOST_ROOT "C:/Users/js_77/boost_1_79_0")
set(BOOSTROOT "C:/Users/js_77/boost_1_79_0")
find_package(Boost_1_79_0)
set(Boost_INCLUDE_DIR C:/Users/js_77/boost_1_79_0)
set(Boost_LIBRARY_DIR C:/Users/js_77/boost_1_79_0/stage/lib)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS "-fexec-charset=CP949")

add_executable(IOCPTest main.cpp AsioSection/BoostAsio.cpp AsioSection/BoostAsio.h)
