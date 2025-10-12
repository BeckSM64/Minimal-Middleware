cmake_minimum_required(VERSION 3.0)
project(MiddlewareTest)

# Use C++11 standard (compatible with older code)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executables
add_executable(broker ${CMAKE_SOURCE_DIR}/broker/broker.cpp)
add_executable(client ${CMAKE_SOURCE_DIR}/app/publish.cpp)

# Link
target_link_libraries(broker pthread)
target_link_libraries(client pthread)
