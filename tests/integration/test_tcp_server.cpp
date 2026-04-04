#include <gtest/gtest.h>

#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_server.h"

TEST(TcpServerTest, ConstructServer) {
    runtime::net::EventLoop loop;
    runtime::net::InetAddress listen_addr(8080, "127.0.0.1");
    runtime::net::TcpServer server(&loop, listen_addr, "TestEchoServer");

    SUCCEED();
}
