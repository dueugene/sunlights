 
cmake_minimum_required (VERSION 2.8.8)
project(hueplusplus)

set(CMAKE_CXX_COMPILER g++)

find_package(hueplusplus)
find_library(WIRINGPI_LIBRARIES NAMES wiringPi)

add_executable(main main.cpp)

target_link_libraries(main hueplusplusstatic ${WIRINGPI_LIBRARIES})
