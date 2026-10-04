# Build sample apps if requested
if(BUILD_SAMPLE_APPS)
    add_executable(publish ${CMAKE_CURRENT_LIST_DIR}/cpp/Publish.cpp)
    add_executable(subscribe ${CMAKE_CURRENT_LIST_DIR}/cpp/Subscribe.cpp)
    add_executable(publish_c ${CMAKE_CURRENT_LIST_DIR}/c/Publish.c)
    add_executable(subscribe_c ${CMAKE_CURRENT_LIST_DIR}/c/Subscribe.c)
    add_executable(publish_raw ${CMAKE_CURRENT_LIST_DIR}/cpp/PublishRaw.cpp)
    add_executable(subscribe_raw ${CMAKE_CURRENT_LIST_DIR}/cpp/SubscribeRaw.cpp)

    target_include_directories(publish PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(subscribe PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(publish_c PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(subscribe_c PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(publish_raw PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)
    target_include_directories(subscribe_raw PRIVATE ${CMAKE_CURRENT_LIST_DIR}/)

    if(WIN32)
        target_link_libraries(publish PRIVATE mmw)
        target_link_libraries(subscribe PRIVATE mmw)
        target_link_libraries(publish_c PRIVATE mmw)
        target_link_libraries(subscribe_c PRIVATE mmw)
        target_link_libraries(publish_raw PRIVATE mmw)
        target_link_libraries(subscribe_raw PRIVATE mmw)

    elseif(EMSCRIPTEN)
        target_link_libraries(publish PRIVATE mmw)
        target_link_libraries(subscribe PRIVATE mmw)
        target_link_libraries(publish_c PRIVATE mmw)
        target_link_libraries(subscribe_c PRIVATE mmw)
        target_link_libraries(publish_raw PRIVATE mmw)
        target_link_libraries(subscribe_raw PRIVATE mmw)

        target_compile_options(publish PRIVATE
            -pthread
            -matomics
            -mbulk-memory
        )
        target_link_options(publish PRIVATE
            -pthread
            -matomics
            -mbulk-memory
            -lwebsocket.js
            -sASYNCIFY
            -sPTHREAD_POOL_SIZE=4
        )

        target_compile_options(subscribe PRIVATE
            -pthread
            -matomics
            -mbulk-memory
        )
        target_link_options(subscribe PRIVATE
            -pthread
            -matomics
            -mbulk-memory
            -lwebsocket.js
            -sASYNCIFY
            -sPTHREAD_POOL_SIZE=4
        )

        target_compile_options(publish_c PRIVATE
            -pthread
            -matomics
            -mbulk-memory
        )
        target_link_options(publish_c PRIVATE
            -pthread
            -matomics
            -mbulk-memory
            -lwebsocket.js
            -sASYNCIFY
            -sPTHREAD_POOL_SIZE=2
        )

        target_compile_options(subscribe_c PRIVATE
            -pthread
            -matomics
            -mbulk-memory
        )
        target_link_options(subscribe_c PRIVATE
            -pthread
            -matomics
            -mbulk-memory
            -lwebsocket.js
            -sASYNCIFY
            -sPTHREAD_POOL_SIZE=2
        )

        target_compile_options(publish_raw PRIVATE
            -pthread
            -matomics
            -mbulk-memory
        )
        target_link_options(publish_raw PRIVATE
            -pthread
            -matomics
            -mbulk-memory
            -lwebsocket.js
            -sASYNCIFY
            -sPTHREAD_POOL_SIZE=4
        )

        target_compile_options(subscribe_raw PRIVATE
            -pthread
            -matomics
            -mbulk-memory
        )
        target_link_options(subscribe_raw PRIVATE
            -pthread
            -matomics
            -mbulk-memory
            -lwebsocket.js
            -sASYNCIFY
            -sPTHREAD_POOL_SIZE=4
        )

    else()
        target_link_libraries(publish PRIVATE mmw pthread)
        target_link_libraries(subscribe PRIVATE mmw pthread)
        target_link_libraries(publish_c PRIVATE mmw pthread)
        target_link_libraries(subscribe_c PRIVATE mmw pthread)
        target_link_libraries(publish_raw PRIVATE mmw pthread)
        target_link_libraries(subscribe_raw PRIVATE mmw pthread)
    endif()
endif()
