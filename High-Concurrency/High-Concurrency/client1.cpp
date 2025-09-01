#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::io_context& io, const std::string& host, const std::string& port)
        : socket_(io), resolver_(io), host_(host), port_(port) {}

    void start() {
        auto self(shared_from_this());
        resolver_.async_resolve(host_, port_,
            [this, self](boost::system::error_code ec, tcp::resolver::results_type endpoints) {
                if (!ec) {
                    boost::asio::async_connect(socket_, endpoints,
                        [this, self](boost::system::error_code ec, tcp::endpoint) {
                            if (!ec) {
                                do_write();
                                do_read();
                            }
                            else {
                                std::cerr << "connect failed: " << ec.message() << "\n";
                            }
                        });
                }
            });
    }

private:
    void do_write() {
        auto self(shared_from_this());
        std::string msg = "Hello from client\n";
        boost::asio::async_write(socket_, boost::asio::buffer(msg),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    // 可以重複送資料 (模擬存活)
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    do_write();
                }
            });
    }

    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    // 收到 echo，這裡先不打印避免淹螢幕
                    // std::cout << "Server: " << std::string(data_, length);
                    do_read();
                }
            });
    }

    tcp::socket socket_;
    tcp::resolver resolver_;
    std::string host_;
    std::string port_;
    enum { max_length = 1024 };
    char data_[max_length];
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: client <host> <port> <connections>\n";
        return 1;
    }

    std::string host = argv[1];
    std::string port = argv[2];
    int connections = std::atoi(argv[3]);

    boost::asio::io_context io;

    std::vector<std::shared_ptr<ClientSession>> clients;
    for (int i = 0; i < connections; ++i) {
        auto client = std::make_shared<ClientSession>(io, host, port);
        client->start();
        clients.push_back(client);
    }

    // 使用多執行緒跑 io_context，避免卡住
    std::vector<std::thread> threads;
    int thread_count = std::thread::hardware_concurrency();
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&io]() { io.run(); });
    }
    for (auto& t : threads) t.join();

    return 0;
}
