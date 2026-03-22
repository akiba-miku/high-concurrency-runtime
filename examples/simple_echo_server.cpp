#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_server.h"

#include <iostream>
#include <string>

int main() {
    runtime::net::EventLoop loop;
    runtime::net::InetAddress listen_addr(8080);
    runtime::net::TcpServer server(&loop, listen_addr, "EchoServer");

    server.setConnectionCallBack(
        [](const std::shared_ptr<runtime::net::TcpConnection> &conn) {
            std::cout << "[conn]"
                      << conn->name()
                      << " local=" << conn->localAddress().toIpPort()
                      << " peer="  << conn->peerAddress().toIpPort()
                      << '\n';
        }
    );

    server.setMessageCallBack(
        [](const std::shared_ptr<runtime::net::TcpConnection> &conn,
           const std::string &message,
           runtime::time::Timestamp) {
            conn->send(message);
           });
    server.start();

    std::cout << "echo server listen on 127.0.0.1:8080\n";
    loop.loop();
    return 0;
}