cmake_minimum_required (VERSION 3.8)

project(asio_echo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 手動指定 Boost include 路徑
include_directories("C:/Program Files/boost/boost_1_89_0")

#server端
add_executable(server server.cpp)
target_link_libraries(server ws2_32)

#client端
add_executable(client client.cpp)
target_link_libraries(client ws2_32)

#client端(同時持續連線版)
add_executable(client1 client1.cpp)
target_link_libraries(client1 ws2_32)
