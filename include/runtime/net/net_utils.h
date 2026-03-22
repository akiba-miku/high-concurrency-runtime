#pragma once

#include <cstdint>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace runtime::net {

int CreateNonBlockingSocket();

bool SetNonBlocking(int fd);
bool SetCloseOnExec(int fd);
bool SetReuseAddr(int fd, bool on = true);
bool SetReusePort(int fd, bool on = true);
bool TcpNonDelay(int fd, bool on = true);
bool SetKeepAlive(int fd, bool on = true);

void IgnoreSigPipe();

sockaddr_in MakeIPv4Address(const std::string& ip, std::uint16_t port);
std::string ToIp(const sockaddr_in& addr);
std::string ToIpPort(const sockaddr_in& addr);
std::string ToPort(const sockaddr_in& addr);

sockaddr_in GetLocalAddr(int fd);
sockaddr_in GetPeerAddr(int fd);

bool IsSelfConnect(int fd);

}  // namespace runtime::net