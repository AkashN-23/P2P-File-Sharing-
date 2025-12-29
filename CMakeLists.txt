cmake_minimum_required(VERSION 3.10)
project(Mark87 VERSION 4.0)

set(CMAKE_CXX_STANDARD 17)

# Find Packages
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# Create the Executable
add_executable(Mark87 main.cpp)

# Link Libraries (The Glue)
target_link_libraries(Mark87 PRIVATE Boost::system Threads::Threads)

# Optimization (Release Mode)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler Flags for Speed
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Mark87 PRIVATE -O3 -Wall)
endif()