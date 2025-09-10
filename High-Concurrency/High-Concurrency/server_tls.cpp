#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include "writelog.h"
#include <atomic>

std::atomic<int> clients_connections = 0;

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

Logger g_logger("checkserver");

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, ssl::context& ctx)
        : ssl_socket_(std::move(socket), ctx) {}

    void start() {
        auto self = shared_from_this();
        ssl_socket_.async_handshake(ssl::stream_base::server,
            [this, self](boost::system::error_code ec) {
                if (!ec) {
                    do_read();
                }
                else {
                    g_logger.log("Handshake failed: " + ec.message());
                }
            });
    }

private:
    void do_read() {
        auto self = shared_from_this();
        ssl_socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                try {
                    if (!ec) {
                        std::string msg(data_, length);
                        g_logger.log("Server received: " + msg);
                        do_write(length);
                    }
                    else if (ec == boost::asio::error::eof) {
                        //g_logger.log("Client closed connection (EOF)");
                        close();
                    }
                    else if (ec.value() == 10054) { // WSAECONNRESET
                        g_logger.log("Client connection reset (WSAECONNRESET)");
                        close();
                    }
                    else {
                        g_logger.log("Read error: " + ec.message());
                        close();
                    }
                }
                catch (const std::exception& e) {
                    g_logger.log(std::string("Exception in do_read: ") + e.what());
                    close();
                }
            });
    }

    void do_write(std::size_t length) {
        auto self = shared_from_this();
        boost::asio::async_write(
            ssl_socket_, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, std::size_t /*len*/) {
                if (!ec) {
                    do_read();
                }
                else {
                    g_logger.log("Write error: " + ec.message());
                    close();
                }
            });
    }

    void close() {    // 新增 TLS shutdown
        auto self = shared_from_this();
        //g_logger.log("closing.");
        boost::system::error_code ig;
        ssl_socket_.async_shutdown([this, self](const boost::system::error_code& ec) {
            close_TCP();// 關閉 TCP socket 
            });

    }
    void close_TCP() {
        // 關閉 TCP socket
        boost::system::error_code ignored_ec;
        ssl_socket_.lowest_layer().shutdown(tcp::socket::shutdown_both, ignored_ec);
        ssl_socket_.lowest_layer().close(ignored_ec);
        clients_connections--;
        //g_logger.log("Client disconnected. Total connections: " + std::to_string(clients_connections.load()));
    }

    ssl::stream<tcp::socket> ssl_socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class Server {
public:
    Server(boost::asio::io_context& io, unsigned short port, ssl::context& ctx)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), ctx_(ctx) {
        acceptor_.listen(8192);
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), ctx_)->start();
                    clients_connections++;
                    g_logger.log("New client connected. Total connections: " + std::to_string(clients_connections.load()));
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
    ssl::context& ctx_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: server <port>\n";
            return 1;
        }

        boost::asio::io_context io;

        // TLS 1.3 Server Context
        ssl::context ctx(ssl::context::tlsv13_server);

        // 載入憑證 & 私鑰 (請自行生成 server.pem)
        ctx.use_certificate_chain_file("server.pem");
        ctx.use_private_key_file("server.pem", ssl::context::pem);
        // 如果有 dhparam.pem 可以啟用（可選）
        // ctx.use_tmp_dh_file("dhparam.pem");

        Server s(io, static_cast<unsigned short>(std::atoi(argv[1])), ctx);

        std::cout << "TLS 1.3 Echo Server running on port " << argv[1] << "...\n";

        // Thread pool
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