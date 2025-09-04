#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <chrono>
#include <fstream>
#include "writelog.h"

using boost::asio::ip::tcp;

Logger g_logger("checkserver");

std::string getCurrentSystemTime() {
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date[60] = { 0 };
    sprintf(date, "%02d: %02d", (int)ptm->tm_min,(int)ptm->tm_sec);

    return std::string(date);
}

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { do_read(); }

private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (ec == boost::asio::error::eof) {
                    g_logger.log("Client kills itself in reading session");
                    do_exit();
                }
                else if(!ec) {
                    std::string reply(data_, length);
                    std::string date = getCurrentSystemTime();
                    g_logger.log("Server get " + reply +" Server time(MM/SS) " + date);
                    do_write(length);
                }
                else {
                    g_logger.log("Server get error from reading "+ ec.message());
                }
            });
        
    }

    void do_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socket_, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec == boost::asio::error::eof) {
                    g_logger.log("Client kills itself in writing session");
                    socket_.close();
                }
                else if (!ec) {
                    do_read();
                    //do_exit();
                    
                }
                else {
                    g_logger.log("Server get error from writing " + ec.message());
                }
            });

    }
    void do_exit() {
        boost::system::error_code ignored_ec;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket_.close();
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class Server {
public:
    Server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: server <port>\n";
            return 1;
        }
        boost::asio::io_context io;
        Server s(io, std::atoi(argv[1]));
        std::cout << "Server running on port "<< argv[1] <<"...\n";
        
        // 使用多執行緒提升效能
        std::vector<std::thread> threads;
        int thread_count = std::thread::hardware_concurrency();
        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        for (auto& t : threads) t.join();

    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}



