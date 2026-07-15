cmake_minimum_required(VERSION 3.16)

project(FinalProject LANGUAGES C)

# Use C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find pthreads
find_package(Threads REQUIRED)

# Source files
set(SOURCES
    src/Server/server.c
    src/Request/Request.c
    src/ThreadPool/Threadpool.c
    src/helpers/helpers.c
)

# Create executable
add_executable(server ${SOURCES})

# Header files
target_include_directories(server PRIVATE
    include
)

# Compiler warnings
target_compile_options(server PRIVATE
    -Wall
    -Wextra
    -g
)

# Link libraries
target_link_libraries(server PRIVATE
    Threads::Threads
)