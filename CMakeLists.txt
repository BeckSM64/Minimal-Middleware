cmake_minimum_required(VERSION 3.0)
project(MiddlewareTest)

# Use C++11 standard (compatible with older code)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the mw library
add_library(mmw STATIC ${CMAKE_SOURCE_DIR}/src/MMW.cpp)

# Library includes
target_include_directories(mmw PRIVATE ${CMAKE_SOURCE_DIR}/)

# Add executables
add_executable(broker ${CMAKE_SOURCE_DIR}/broker/broker.cpp)
add_executable(client ${CMAKE_SOURCE_DIR}/app/publish.cpp)

# Application includes
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/)

# Link
target_link_libraries(broker pthread)
target_link_libraries(client PRIVATE mmw pthread)
