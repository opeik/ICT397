cmake_minimum_required(VERSION 3.16)

target_sources(${PROJECT_NAME} PRIVATE
    Main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(afk)
