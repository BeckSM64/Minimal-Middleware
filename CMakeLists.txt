cmake_minimum_required(VERSION 3.0)
project(MiddlewareTest)

# Use C++11 standard (compatible with older code)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Get YAML CPP Library
include(FetchContent)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0 # Can be a tag (yaml-cpp-x.x.x), a commit hash, or a branch name (master)
)
FetchContent_MakeAvailable(yaml-cpp)

# Create the mw library
add_library(mmw STATIC ${CMAKE_SOURCE_DIR}/src/MMW.cpp ${CMAKE_SOURCE_DIR}/src/ConfigFileParser.cpp)
target_link_libraries(mmw PUBLIC yaml-cpp::yaml-cpp)

# Library includes
target_include_directories(mmw PRIVATE ${CMAKE_SOURCE_DIR}/ ${CMAKE_SOURCE_DIR}/includes/)

# Add executables
add_executable(broker ${CMAKE_SOURCE_DIR}/broker/Broker.cpp)
add_executable(publish ${CMAKE_SOURCE_DIR}/app/Publish.cpp)
add_executable(subscribe ${CMAKE_SOURCE_DIR}/app/Subscribe.cpp)

# Application includes
target_include_directories(publish PRIVATE ${CMAKE_SOURCE_DIR}/)
target_include_directories(subscribe PRIVATE ${CMAKE_SOURCE_DIR}/)

# Link
target_link_libraries(broker pthread)
target_link_libraries(publish PRIVATE mmw pthread)
target_link_libraries(subscribe PRIVATE mmw pthread)
