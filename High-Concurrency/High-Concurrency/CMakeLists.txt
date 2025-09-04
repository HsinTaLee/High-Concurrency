# CMakeList.txt: Serverside 的 CMake 專案，在此包含來源及定義
# 專案專屬邏輯。
#
cmake_minimum_required (VERSION 3.8)

# 將來源新增到此專案的可執行檔。
#add_executable (Serverside "server.cpp" "Serverside.h" "client.cpp" "server_nothread.cpp" "server_IOCP.cpp" "server_IOCP.cpp" "server_asio.cpp" "client_asio.cpp")

# TODO: 新增測試，並視需要安裝目標。



#cmake_minimum_required(VERSION 3.15)
project(asio_echo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 手動指定 Boost include 路徑
include_directories("C:/Program Files/boost/boost_1_89_0")

add_executable(server server.cpp writelog.h)
target_link_libraries(server ws2_32)

add_executable(client client.cpp writelog.h)
target_link_libraries(client ws2_32)
