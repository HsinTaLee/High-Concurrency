#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include "writelog.h"
#include <atomic>

std::atomic<int> counter = 0;
using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
namespace chrono = boost::asio::chrono;

Logger g_logger("checkclient");

std::string getCurrentSystemTime() {
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char buf[60] = { 0 };
    sprintf(buf, "%02d:%02d", (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(buf);
}

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::io_context& io, ssl::context& ssl_ctx,
        std::string msg, int repeat_count, int interval_ms)
        : socket_(io, ssl_ctx),
        message_(std::move(msg)),
        timer_(io),
        remaining_(repeat_count),
        interval_ms_(interval_ms) {}

    void start(const tcp::resolver::results_type& endpoints) {
        auto self = shared_from_this();
        boost::asio::async_connect(
            socket_.lowest_layer(), endpoints,
            [this, self, endpoints](boost::system::error_code ec, tcp::endpoint) {
                if (!ec) {
                    // 完成 TCP connect，開始 TLS handshake
                    socket_.async_handshake(ssl::stream_base::client,
                        [this, self](boost::system::error_code ec2) {
                            if (!ec2) {
                                counter++;
                                do_one_cycle();
                            }
                            else {
                                g_logger.log("TLS handshake failed: " + ec2.message());
                                close();
                            }
                        });
                }
                else {
                    auto self2 = shared_from_this();
                    g_logger.log("TCP connect failed: " + ec.message() + " | " + message_);
                    schedule_reconnect(endpoints);
                }
            });

    }

private:
    void do_one_cycle() {
        if (remaining_ < 0) {
            g_logger.log("Done cycles, closing: " + message_);
            close();
            return;
        }
        else if (remaining_ == 0) remaining_ = 100; // remaining== 0 = 100次

        auto self = shared_from_this();
        boost::asio::async_write(
            socket_, boost::asio::buffer(message_),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    g_logger.log("Write error: " + ec.message() + " | " + message_);
                    close();
                    return;
                }
                async_read_reply();
            });
    }

    void async_read_reply() {
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_, boost::asio::buffer(reply_, message_.size()),
            [this, self](boost::system::error_code ec, std::size_t n) {
                if (ec) {
                    g_logger.log("Read error: " + ec.message() + " | " + message_);
                    close();
                    return;
                }

                std::string r(reply_, n);
                if (r == message_) {
                    g_logger.log("Echo OK | " + message_ + " | T=" + getCurrentSystemTime());
                }
                else {
                    g_logger.log("Echo mismatch | expect='" + message_ + "' got='" + r + "'");
                }

                --remaining_;
                if (remaining_ > 0) {
                    auto self2 = shared_from_this();
                    timer_.expires_after(chrono::milliseconds(interval_ms_));
                    timer_.async_wait([this, self2](boost::system::error_code tec) {
                        if (!tec) {
                            do_one_cycle();
                        }
                        else if (tec != boost::asio::error::operation_aborted) {
                            g_logger.log("Timer error: " + tec.message() + " | " + message_);
                            close();
                        }
                        });
                }
                else {
                    //g_logger.log(message_+" Last connection, closing.");
                    close();
                }
            });
    }

    void close() {
        // 新增 TLS shutdown
        auto self(shared_from_this());
        int timer_ms = 1000; // 1秒後強制關閉
        timer_.expires_after(chrono::milliseconds(timer_ms));
        timer_.async_wait([this, self](boost::system::error_code tec) {
            if (!tec) {
                auto self2(shared_from_this());
                socket_.async_shutdown([this, self2](const boost::system::error_code& ec) {
                    close_TCP();// 關閉 TCP socket
                    });
            }
            else {
                g_logger.log("close error: " + tec.message() + " | " + message_);
            }
            });


    }
    void close_TCP() {
        auto self(shared_from_this());
        // 關閉 TCP socket
        boost::system::error_code ig;
        socket_.lowest_layer().shutdown(tcp::socket::shutdown_both, ig);
        socket_.lowest_layer().close(ig);
        counter--;
        //g_logger.log("closed. counter=" + std::to_string(counter.load()));
    }

    void schedule_reconnect(const tcp::resolver::results_type& endpoints) {
        auto self(shared_from_this());
        timer_.expires_after(chrono::seconds(10));  // 等 10 秒
        timer_.async_wait([this, self, endpoints](boost::system::error_code ec) {
            if (!ec) {
                g_logger.log("Retrying connect after 10s: " + message_);
                socket_.lowest_layer().close();              // 確保 socket 清乾淨
                socket_.lowest_layer().open(tcp::v4());      // 重新開
                start(endpoints);                            // 再呼叫一次 start()
            }
            });
    }

    ssl::stream<tcp::socket> socket_;
    std::string message_;
    char reply_[1024];
    boost::asio::steady_timer timer_;
    int remaining_;
    int interval_ms_;
};

int main(int argc, char* argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: client <host> <port> <num_connections_per_tick> <ticks> <write_read_cycles> <interval_ms>\n";
        return 1;
    }

    const std::string host = argv[1];
    const std::string port = argv[2];
    const int per_tick = std::stoi(argv[3]);
    const int ticks = std::stoi(argv[4]);
    const int cycles_per_conn = std::stoi(argv[5]);
    const int interval_ms = std::stoi(argv[6]);

    boost::asio::io_context io;

    // TLS context (client)
    ssl::context ssl_ctx(ssl::context::tlsv13_client);
    ssl_ctx.set_default_verify_paths();
    ssl_ctx.set_verify_mode(ssl::verify_none); // 測試用，正式環境要 verify_peer

    auto guard = boost::asio::make_work_guard(io);

    // Thread pool
    std::vector<std::thread> threads;
    //int thread_count =  std::thread::hardware_concurrency();
    int thread_count = 8; // 固定8個thread


    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&io]() { io.run(); });
    }

    tcp::resolver resolver(io);
    auto endpoints = resolver.resolve(host, port);

    int global_id = 0;
    for (int t = 0; t < ticks; ++t) {
        for (int i = 0; i < per_tick; ++i) {
            const int id = ++global_id;
            const std::string date = getCurrentSystemTime();
            const std::string msg = "Client " + std::to_string(id) + " Time(MM/SS) " + date + " ";

            boost::asio::post(io, [&, msg]() {
                auto s = std::make_shared<ClientSession>(io, ssl_ctx, msg, cycles_per_conn, interval_ms);
                s->start(endpoints);
                });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (1) {
        if (counter.load() == 0) {
            guard.reset();
            break;
        }
    }
    for (auto& th : threads) th.join();

    return 0;
}