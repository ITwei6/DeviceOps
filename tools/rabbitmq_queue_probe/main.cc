#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <ev.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

int parseTimeoutMs(const char* text) {
    try {
        return std::stoi(text);
    } catch (...) {
        return 10000;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "usage: rabbitmq_queue_probe <amqp_url> <exchange> <queue> <routing_key> <contains> [timeout_ms]\n";
        return 2;
    }

    const std::string url = argv[1];
    const std::string queue = argv[3];
    const std::string expected = argv[5];
    const int timeout_ms = argc >= 7 ? parseTimeoutMs(argv[6]) : 10000;

    struct ev_loop* loop = ev_loop_new(0);
    if (loop == nullptr) {
        std::cerr << "failed to create ev loop\n";
        return 2;
    }

    AMQP::LibEvHandler handler(loop);
    AMQP::TcpConnection connection(&handler, AMQP::Address(url));
    AMQP::TcpChannel channel(&connection);

    bool matched = false;
    bool failed = false;
    int consumed = 0;
    std::string error;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    std::function<void()> get_next;
    get_next = [&]() {
        if (matched || failed) {
            ev_break(loop, EVBREAK_ALL);
            return;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            ev_break(loop, EVBREAK_ALL);
            return;
        }

        channel.get(queue, AMQP::noack)
            .onSuccess([&](const AMQP::Message& message, uint64_t, bool) {
                ++consumed;
                const std::string body(message.body(), message.bodySize());
                if (body.find(expected) != std::string::npos) {
                    matched = true;
                    ev_break(loop, EVBREAK_ALL);
                    return;
                }
                get_next();
            })
            .onEmpty([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                get_next();
            })
            .onError([&](const char* message) {
                failed = true;
                error = message == nullptr ? "unknown error" : message;
                ev_break(loop, EVBREAK_ALL);
            });
    };

    channel.onReady([&]() {
        get_next();
    });
    channel.onError([&](const char* message) {
        failed = true;
        error = message == nullptr ? "unknown error" : message;
        ev_break(loop, EVBREAK_ALL);
    });

    ev_run(loop, 0);

    if (matched) {
        std::cout << "rabbitmq queue ok: " << queue << ", consumed=" << consumed << "\n";
        std::cout.flush();
        std::_Exit(0);
    }
    if (failed) {
        std::cerr << "rabbitmq queue probe failed: queue=" << queue << ", error=" << error << "\n";
        std::cerr.flush();
        std::_Exit(1);
    }

    std::cerr << "no matching message: queue=" << queue
              << ", contains=" << expected
              << ", consumed=" << consumed << "\n";
    std::cerr.flush();
    std::_Exit(1);
}
