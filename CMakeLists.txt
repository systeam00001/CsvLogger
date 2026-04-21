cmake_minimum_required(VERSION 3.16)
project(csv_logger_example LANGUAGES CXX)

add_executable(csv_logger_example
    example_main.cpp
    CsvLogger.cpp
)

target_include_directories(csv_logger_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(csv_logger_example PRIVATE cxx_std_17)

target_link_libraries(csv_logger_example PRIVATE pthread)
