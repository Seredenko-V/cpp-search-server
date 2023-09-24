cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDART 17)

project(search-server)

set(HEADERS concurrent_map.h document.h log_duration.h paginator.h process_queries.h read_input_functions.h
    remove_duplicates.h request_queue.h search_server.h string_processing.h)

set(SOURCES document.cpp process_queries.cpp read_input_functions.cpp remove_duplicates.cpp request_queue.cpp
    search_server.cpp string_processing.cpp)

set(TEST_FILES tests.h tests.cpp)

add_executable(SearchServer ${HEADERS} ${SOURCES} ${TEST_FILES} main.cpp)
