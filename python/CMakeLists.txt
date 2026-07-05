include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v3.0.1
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(_mmw Bindings.cpp)

target_link_libraries(_mmw PRIVATE mmw)

target_include_directories(_mmw PRIVATE
    ${PROJECT_SOURCE_DIR}/includes
)

install(TARGETS _mmw
    LIBRARY DESTINATION mmw
    RUNTIME DESTINATION mmw
)
