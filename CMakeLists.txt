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
add_executable(publish ${CMAKE_SOURCE_DIR}/app/Publish.cpp)
add_executable(subscribe ${CMAKE_SOURCE_DIR}/app/Subscribe.cpp)

# Application includes
target_include_directories(publish PRIVATE ${CMAKE_SOURCE_DIR}/)
target_include_directories(subscribe PRIVATE ${CMAKE_SOURCE_DIR}/)

# Link
target_link_libraries(broker pthread)
target_link_libraries(publish PRIVATE mmw pthread)
target_link_libraries(subscribe PRIVATE mmw pthread)
