cmake_minimum_required(VERSION 3.0)
project(MiddlewareTest)

# Use C++11 standard (compatible with older code)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options for what to build
option(BUILD_BROKER "Build broker executable" OFF)
option(BUILD_SAMPLE_APPS "Build sample apps (publish/subscribe etc.)" OFF)
option(BUILD_PYTHON_MODULE "Build Python bindings" OFF)

# Get YAML CPP Library
include(FetchContent)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

# Get JSON library
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# Get SPD Log Library
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# Fetch pybind11
set(PYBIND11_FINDPYTHON NEW CACHE STRING "")
set(PYBIND11_PYTHON_VERSION 3.12 CACHE STRING "")
set(_Python_EXECUTABLE_DEBUG "" CACHE FILEPATH "Suppress debug executable error")
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v3.0.1
)
FetchContent_MakeAvailable(pybind11)

# Fetch Cereal
set(CEREAL_USE_BOOST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  cereal
  GIT_REPOSITORY https://github.com/USCiLab/cereal.git
  GIT_TAG v1.3.2
)
FetchContent_Populate(cereal)

# Set the serializer
if(CEREAL_SERIALIZER)
    add_compile_definitions(CEREAL_SERIALIZER)
    set(SERIALIZER_SRC ${CMAKE_CURRENT_LIST_DIR}/src/serialization/CerealSerializer.cpp)
elseif(JSON_SERIALIZER)
    add_compile_definitions(JSON_SERIALIZER)
    set(SERIALIZER_SRC ${CMAKE_CURRENT_LIST_DIR}/src/serialization/JsonSerializer.cpp)
else()
    message(WARNING "No serializer provided, using CEREAL_SERIALIZER by default")
    add_compile_definitions(CEREAL_SERIALIZER)
    set(SERIALIZER_SRC ${CMAKE_CURRENT_LIST_DIR}/src/serialization/CerealSerializer.cpp)
endif()

# Add BUILD_SHARED_LIBS option
option(BUILD_SHARED_LIBS "Build as shared library" ON)

# Create the mw library
add_library(
    mmw
    ${CMAKE_CURRENT_LIST_DIR}/src/MMW.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/ConfigFileParser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/serialization/SerializerAbstraction.cpp
    ${SERIALIZER_SRC}
    ${CMAKE_CURRENT_LIST_DIR}/src/network/SocketAbstraction.cpp
)

# Create an alias target
add_library(mmw::mmw ALIAS mmw)

# Export include directories
target_include_directories(mmw PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(mmw PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ ${CMAKE_CURRENT_LIST_DIR}/includes/ ${cereal_SOURCE_DIR}/include/)

# Link Libraries
if(WIN32)
    target_link_libraries(mmw PUBLIC yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json spdlog::spdlog_header_only ws2_32)
else()
    target_link_libraries(mmw PUBLIC yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json spdlog::spdlog_header_only)
endif()

# Build broker if requested
if(BUILD_BROKER)
    add_executable(
        broker ${CMAKE_CURRENT_LIST_DIR}/broker/Broker.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/serialization/SerializerAbstraction.cpp
        ${SERIALIZER_SRC}
        ${CMAKE_CURRENT_LIST_DIR}/src/network/SocketAbstraction.cpp
    )
    target_include_directories(broker PRIVATE ${CMAKE_CURRENT_LIST_DIR}/includes/ ${cereal_SOURCE_DIR}/include/)
    if(WIN32)
        target_link_libraries(broker nlohmann_json::nlohmann_json pthread spdlog::spdlog_header_only ws2_32)
    else()
        target_link_libraries(broker nlohmann_json::nlohmann_json pthread spdlog::spdlog_header_only)
    endif()
endif()

# Build sample apps if requested
if(BUILD_SAMPLE_APPS)
    add_executable(publish ${CMAKE_CURRENT_LIST_DIR}/app/Publish.cpp)
    add_executable(subscribe ${CMAKE_CURRENT_LIST_DIR}/app/Subscribe.cpp)
    add_executable(publish_c ${CMAKE_CURRENT_LIST_DIR}/app/Publish.c)
    add_executable(subscribe_c ${CMAKE_CURRENT_LIST_DIR}/app/Subscribe.c)
    add_executable(publish_raw ${CMAKE_CURRENT_LIST_DIR}/app/PublishRaw.cpp)
    add_executable(subscribe_raw ${CMAKE_CURRENT_LIST_DIR}/app/SubscribeRaw.cpp)

    target_include_directories(publish PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(subscribe PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(publish_c PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(subscribe_c PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(publish_raw PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(subscribe_raw PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)

    target_link_libraries(publish PRIVATE mmw pthread)
    target_link_libraries(subscribe PRIVATE mmw pthread)
    target_link_libraries(publish_c PRIVATE mmw pthread)
    target_link_libraries(subscribe_c PRIVATE mmw pthread)
    target_link_libraries(publish_raw PRIVATE mmw pthread)
    target_link_libraries(subscribe_raw PRIVATE mmw pthread)

    # Copy config if it exists
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/config/config.yml")
        file(COPY "${CMAKE_CURRENT_LIST_DIR}/config/config.yml" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/build/")
    endif()
endif()

# Build Python module if requested
if(BUILD_PYTHON_MODULE)
    pybind11_add_module(mmw_python python/Bindings.cpp)
    target_include_directories(mmw_python PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR}/ 
        ${CMAKE_CURRENT_LIST_DIR}/includes/
    )
    target_link_libraries(mmw_python PRIVATE mmw)
    set_target_properties(mmw_python PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/python"
    )
endif()

# Copy Cereal license
file(COPY ${cereal_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_CURRENT_LIST_DIR}/third_party_licenses/)
file(RENAME ${CMAKE_CURRENT_LIST_DIR}/third_party_licenses/LICENSE ${CMAKE_CURRENT_LIST_DIR}/third_party_licenses/cereal_LICENSE.txt)
