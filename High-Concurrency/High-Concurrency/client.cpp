#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>

using boost::asio::ip::tcp;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::io_context& io, const std::string& msg)
        : socket_(io), message_(msg) {}

    void start(tcp::resolver::results_type endpoints) {
        auto self(shared_from_this());
        boost::asio::async_connect(socket_, endpoints,
            [this, self](boost::system::error_code ec, tcp::endpoint) {
                if (!ec) {
                    do_write();
                }
            });
    }

private:
    void do_write() {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(message_),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    do_read();
                }
            });
    }

    void do_read() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(reply_, message_.size()),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string reply(reply_, length);
                    if (reply == message_) {
                        // echo ok
                        // 可以在這裡再發送一次測試
                        do_write();
                    }
                    else {
                        std::cerr << "Echo mismatch!" << std::endl;
                    }
                }
            });
    }

    tcp::socket socket_;
    std::string message_;
    char reply_[1024];
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: client <host> <port> <num_connections>\n";
        return 1;
    }

    std::string host = argv[1];
    std::string port = argv[2];
    int num_clients = std::stoi(argv[3]);

    boost::asio::io_context io;
    tcp::resolver resolver(io);
    auto endpoints = resolver.resolve(host, port);

    std::vector<std::shared_ptr<ClientSession>> clients;
    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_shared<ClientSession>(io, "Hello from client " + std::to_string(i));
        client->start(endpoints);
        clients.push_back(client);
    }

    // 多線程跑 io_context，避免單線程卡死
    std::vector<std::thread> threads;
    int thread_count = std::thread::hardware_concurrency();
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&io]() { io.run(); });
    }

    for (auto& t : threads) t.join();
}