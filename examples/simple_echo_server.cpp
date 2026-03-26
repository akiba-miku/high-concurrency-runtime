#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_server.h"
#include "runtime/time/timestamp.h"

#include <iostream>
#include <memory>
#include <string>

int main() {
    runtime::net::EventLoop main_loop;
    runtime::net::InetAddress listen_addr(8080, "127.0.0.1");

    runtime::net::TcpServer server(&main_loop, listen_addr, "EchoServer");

    // 先开 4 个 IO 线程，one loop per thread
    server.setThreadNum(4);

    server.setConnectionCallBack(
        [](const std::shared_ptr<runtime::net::TcpConnection>& conn) {
            std::cout << "[connection] "
                      << conn->name()
                      << " local=" << conn->localAddress().toIpPort()
                      << " peer=" << conn->peerAddress().toIpPort()
                      << '\n';
        });

    server.setMessageCallBack(
        [](const std::shared_ptr<runtime::net::TcpConnection>& conn,
           const std::string& message,
           runtime::time::Timestamp receive_time) {
            std::cout << "[message] "
                      << conn->name()
                      << " at " << receive_time.toFormattedString()
                      << " => " << message;

            conn->send(message);
        });

    server.setWriteCompleteCallBack(
        [](const std::shared_ptr<runtime::net::TcpConnection>& conn) {
            std::cout << "[write complete] " << conn->name() << '\n';
        });

    server.start();

    std::cout << "echo server listen on 127.0.0.1:8080\n";
    main_loop.loop();
    return 0;
}
