#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include "writelog.h"

using boost::asio::ip::tcp;

Logger g_logger("checkclient");

std::string getCurrentSystemTime() {
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date[60] = { 0 };
    sprintf(date, "%02d: %02d", (int)ptm->tm_min, (int)ptm->tm_sec);

    return std::string(date);
}

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::io_context& io, const std::string& msg, int doboth)
        : socket_(io), message_(msg) , doboth_(doboth){}

    void start(tcp::resolver::results_type endpoints) {
        auto self(shared_from_this());
        boost::asio::async_connect(socket_, endpoints,
            [this, self](boost::system::error_code ec, tcp::endpoint) {  
                if (!ec) {
                    //g_logger.log(message_);
                    do_both(doboth_); //裡面數字代表 do_write->do-read 重複n次
                }
                else {
                    g_logger.log(message_ +"fullllll" + ec.message());
                }
            });
    }

private:
    void do_write() {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(message_),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (ec == boost::asio::error::eof) {
                    g_logger.log("server killed himself in writing session");
                    socket_.close();
                }
                else if (!ec) {
                    //do_read();
                }
                else {
                    g_logger.log(message_ + "writing fail");
                }
            });
    }
    void do_read() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(reply_, message_.size()),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (ec == boost::asio::error::eof) {
                    g_logger.log("server killed himself in reading session");
                    socket_.close();
                }
                else if (!ec) {
                    std::string reply(reply_, length);
                    if (reply == message_) {
                        std::string date = getCurrentSystemTime();
                        g_logger.log("Echo OK, closing: " + message_ + "Client time = " + date );
                        // 主動關閉連線
                        //do_exit();
                    }
                    else {
                        g_logger.log("Echo mismatch! client: " + message_);
                    }
                }
                else {
                    g_logger.log("Read error on " + message_ + ": " + ec.message());
                }
            });

    }
    void do_exit() {
        boost::system::error_code ignored_ec;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket_.close();
    }
    void do_both(int j) {
        if (j ==  0) {
            j = 100;
        }
        for (int i = 0; i < j; i++) {
            do_write();
            do_read();
        }
    }

    tcp::socket socket_;
    std::string message_;
    char reply_[1024];
    int doboth_;
};


int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: client <host> <port> <num_connections/t><multi/t><write->read/t>\n";
        return 1;
    }
    std::string host = argv[1];
    std::string port = argv[2];
    int num_clients = std::stoi(argv[3]);
    int num_limit = std::stoi(argv[4]);
    int num_trade = std::stoi(argv[5]);

    boost::asio::io_context io;
    tcp::resolver resolver(io);
    auto endpoints = resolver.resolve(host, port);
    int num = 0;
    for (int multi = 0; multi < num_limit; ++multi) {
        std::vector<std::shared_ptr<ClientSession>> clients;
        for (int i = 0; i < num_clients; ++i) {
            std::string date = getCurrentSystemTime();
            std::string msg = "Client " + std::to_string(num) + "     Time(MM/SS) " + date + "      ";
            auto client = std::make_shared<ClientSession>(io, msg, num_trade);
            //boost::asio::post(io, [&, msg]() {
            
            client->start(endpoints);
           //   });
            clients.push_back(client);
            num=num + 1 ;
        }
        // 多線程跑 io_context，避免單線程卡死
        int thread_count = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        for (int a = 0; a < thread_count; ++a) {
            //g_logger.log(std::to_string(a) + "'s Thread activated!");
            threads.emplace_back([&io]() {io.run();});
        }
        for (auto& t : threads) {
            t.join();
            //g_logger.log(std::to_string(multi)+"'s Join done");
        }
        //if (io.stopped())g_logger.log("jsafio");

    }
    //Sleep(10000);
}