cmake_minimum_required(VERSION 3.16)
project(Mark8_FileTransfer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fixes the "Policy CMP0167" warning
set(Boost_NO_WARN_NEW_VERSIONS 1)

# Find Packages
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)
find_package(Boost REQUIRED COMPONENTS system thread) # Added 'thread' component
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

# --- CLIENT ---
add_executable(client client.cpp)
target_link_libraries(client PRIVATE
    Qt6::Core Qt6::Network Qt6::Widgets
    Boost::system Boost::thread
    OpenSSL::SSL OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# --- SERVER ---
add_executable(server signal_server.cpp)
target_link_libraries(server PRIVATE
    Boost::system Boost::thread
    nlohmann_json::nlohmann_json
)