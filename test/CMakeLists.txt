# Build sample apps if requested
add_executable(MMWScaleTest ${PROJECT_SOURCE_DIR}/test/MMWScaleTest.cpp)
target_include_directories(MMWScaleTest PRIVATE ${PROJECT_SOURCE_DIR}/)
target_link_libraries(MMWScaleTest PRIVATE mmw pthread)
