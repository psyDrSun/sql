cmake_minimum_required(VERSION 3.15)
project(mini_dbms VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    src/main.cpp
    src/CLIHandler.cpp
    src/SQLParser.cpp
    src/ExecutionEngine.cpp
    src/CatalogManager.cpp
    src/StorageManager.cpp
    src/Types.cpp
)

add_executable(mini_dbms ${SOURCES})

target_include_directories(mini_dbms
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)
