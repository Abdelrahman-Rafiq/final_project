cmake_minimum_required(VERSION 3.16)
project(FinalProject LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(_POSIX_C_SOURCE=200809L _GNU_SOURCE)

find_package(Threads REQUIRED)

set(SOURCES
    src/main.c
    src/helpers.c
    src/CacheLayer/Cache.c
    src/CacheLayer/DLL.c
    src/CacheLayer/HashTable.c
    src/config/Config.c
    src/Server/Request.c
    src/Server/server.c
    src/ThreadPool/threadpool.c
)

add_executable(server ${SOURCES})

execute_process(
    COMMAND gcc -print-multiarch
    OUTPUT_VARIABLE MULTIARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(MULTIARCH)
    target_include_directories(server PRIVATE
        include
        /usr/include/${MULTIARCH}
    )
else()
    target_include_directories(server PRIVATE include)
endif()
target_compile_options(server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -g
)

add_custom_target(run
    COMMAND ./server
    DEPENDS server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running server..."
)

target_link_libraries(server PRIVATE Threads::Threads)