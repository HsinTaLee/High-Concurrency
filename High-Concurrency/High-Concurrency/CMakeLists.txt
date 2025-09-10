# CMakeList.txt: Serverside 的 CMake 專案，在此包含來源及定義
# 專案專屬邏輯。
#
cmake_minimum_required (VERSION 3.15)

# 將來源新增到此專案的可執行檔。
#add_executable (Serverside "server.cpp" "Serverside.h" "client.cpp" "server_nothread.cpp" "server_IOCP.cpp" "server_IOCP.cpp" "server_asio.cpp" "client_asio.cpp")

# TODO: 新增測試，並視需要安裝目標。



#cmake_minimum_required(VERSION 3.15)
project(tcp_tls1.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 手動指定 Boost include 路徑
include_directories("C:/Program Files/boost/boost_1_89_0")
# OpenSSL
# 指定 OpenSSL root
set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
if(CMAKE_BUILD_TYPE MATCHES Debug)
set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/x64/MDd")
else()
set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/x64/MD")
endif()

# 明確告訴 CMake 要用的 lib
set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_LIB_DIR}/libcrypto.lib")
set(OPENSSL_SSL_LIBRARY "${OPENSSL_LIB_DIR}/libssl.lib")

find_package(OpenSSL REQUIRED)


add_executable(server server.cpp writelog.h)
target_link_libraries(server ws2_32)

add_executable(client client.cpp writelog.h)
target_link_libraries(client ws2_32)

add_executable(server_tls server_tls.cpp writelog.h)
target_include_directories(server_tls PRIVATE ${OPENSSL_INCLUDE_DIR})
#target_link_libraries(server ws2_32)
target_link_libraries(server_tls PRIVATE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})


add_executable(client_tls client_tls.cpp writelog.h)
target_include_directories(client_tls PRIVATE ${OPENSSL_INCLUDE_DIR})
#target_link_libraries(client ws2_32)
target_link_libraries(client_tls PRIVATE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
