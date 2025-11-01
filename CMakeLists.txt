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

# Get JSON library
include(FetchContent)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# Get SPD Log Library
include(FetchContent)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# Fetch pybind11
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v3.0.1
)
FetchContent_MakeAvailable(pybind11)

# Fetch Cereal
FetchContent_Declare(
  cereal
  GIT_REPOSITORY https://github.com/USCiLab/cereal.git
  GIT_TAG v1.3.2
)
FetchContent_Populate(cereal)

# Build the Python module
pybind11_add_module(mmw_python python/Bindings.cpp)
target_include_directories(mmw_python PRIVATE 
    ${CMAKE_SOURCE_DIR}/ 
    ${CMAKE_SOURCE_DIR}/includes/
)
target_link_libraries(mmw_python PRIVATE mmw)

# Force output into the python/ folder
set_target_properties(mmw_python PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/python"
)

# Set the serializer
if(CEREAL_SERIALIZER)
    add_compile_definitions(CEREAL_SERIALIZER)
elseif(JSON_SERIALIZER)
    add_compile_definitions(JSON_SERIALIZER)
else()
    message(FATAL_ERROR "You must define either CEREAL_SERIALIZER or JSON_SERIALIZER")
endif()

# Add BUILD_SHARED_LIBS option
option(BUILD_SHARED_LIBS "Build as shared library" ON)

# Create the mw library
add_library(
    mmw
    ${CMAKE_SOURCE_DIR}/src/MMW.cpp
    ${CMAKE_SOURCE_DIR}/src/ConfigFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/serialization/SerializerAbstraction.cpp
    ${CMAKE_SOURCE_DIR}/src/serialization/JsonSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/serialization/CerealSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/network/SocketAbstraction.cpp
)

# Link Libraries, only link ws2_32 if we're on Windows
if(WIN32)
  target_link_libraries(mmw PUBLIC yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json spdlog::spdlog_header_only ws2_32)
else(WIN32)
  target_link_libraries(mmw PUBLIC yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json spdlog::spdlog_header_only)
endif(WIN32)

# Library includes
target_include_directories(mmw PRIVATE ${CMAKE_SOURCE_DIR}/ ${CMAKE_SOURCE_DIR}/includes/ ${cereal_SOURCE_DIR}/include/)

# Add executables
add_executable(
  broker ${CMAKE_SOURCE_DIR}/broker/Broker.cpp
  ${CMAKE_SOURCE_DIR}/src/serialization/SerializerAbstraction.cpp
  ${CMAKE_SOURCE_DIR}/src/serialization/JsonSerializer.cpp
  ${CMAKE_SOURCE_DIR}/src/serialization/CerealSerializer.cpp
  ${CMAKE_SOURCE_DIR}/src/network/SocketAbstraction.cpp
)
add_executable(publish ${CMAKE_SOURCE_DIR}/app/Publish.cpp)
add_executable(subscribe ${CMAKE_SOURCE_DIR}/app/Subscribe.cpp)
add_executable(publish_c ${CMAKE_SOURCE_DIR}/app/Publish.c)
add_executable(subscribe_c ${CMAKE_SOURCE_DIR}/app/Subscribe.c)
add_executable(publish_raw ${CMAKE_SOURCE_DIR}/app/PublishRaw.cpp)
add_executable(subscribe_raw ${CMAKE_SOURCE_DIR}/app/SubscribeRaw.cpp)

# Application includes
target_include_directories(publish PRIVATE ${CMAKE_SOURCE_DIR}/)
target_include_directories(subscribe PRIVATE ${CMAKE_SOURCE_DIR}/)
target_include_directories(broker PRIVATE ${CMAKE_SOURCE_DIR}/includes/ ${cereal_SOURCE_DIR}/include/)
target_include_directories(publish_c PRIVATE ${CMAKE_SOURCE_DIR}/)
target_include_directories(subscribe_c PRIVATE ${CMAKE_SOURCE_DIR}/)
target_include_directories(publish_raw PRIVATE ${CMAKE_SOURCE_DIR}/)
target_include_directories(subscribe_raw PRIVATE ${CMAKE_SOURCE_DIR}/)

# Link Libraries, only link ws2_32 if we're on Windows
if(WIN32)
  target_link_libraries(broker nlohmann_json::nlohmann_json pthread spdlog::spdlog_header_only ws2_32)
else(WIN32)
  target_link_libraries(broker nlohmann_json::nlohmann_json pthread spdlog::spdlog_header_only)
endif(WIN32)
target_link_libraries(publish PRIVATE mmw pthread)
target_link_libraries(subscribe PRIVATE mmw pthread)
target_link_libraries(publish_c PRIVATE mmw pthread)
target_link_libraries(subscribe_c PRIVATE mmw pthread)
target_link_libraries(publish_raw PRIVATE mmw pthread)
target_link_libraries(subscribe_raw PRIVATE mmw pthread)

# Copy config to build directory for sample apps
file(COPY "${CMAKE_SOURCE_DIR}/config/config.yml" DESTINATION "${CMAKE_SOURCE_DIR}/build/")

# Copy Cereal's license into third party license folder
file(COPY ${cereal_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_SOURCE_DIR}/third_party_licenses/)
file(RENAME ${CMAKE_SOURCE_DIR}/third_party_licenses/LICENSE ${CMAKE_SOURCE_DIR}/third_party_licenses/cereal_LICENSE.txt)
