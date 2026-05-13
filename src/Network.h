#ifndef NETWORK_H
#define NETWORK_H

#include <bits/stdc++.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SocketHandle = SOCKET;
static const SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
static const SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

using namespace std;

class NetworkRuntime {
public:
    NetworkRuntime() {
#ifdef _WIN32
        WSADATA data;
        if(WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw runtime_error("WSAStartup failed");
        }
#endif
    }

    ~NetworkRuntime() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

inline void close_socket(SocketHandle sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

inline bool send_all(SocketHandle sock, const string& data) {
    const char* buf = data.c_str();
    size_t total = 0;
    while(total < data.size()) {
#ifdef _WIN32
        int sent = send(sock, buf + total, static_cast<int>(data.size() - total), 0);
#else
        ssize_t sent = send(sock, buf + total, data.size() - total, 0);
#endif
        if(sent <= 0) return false;
        total += static_cast<size_t>(sent);
    }
    return true;
}

inline void shutdown_socket_write(SocketHandle sock) {
#ifdef _WIN32
    shutdown(sock, SD_SEND);
#else
    shutdown(sock, SHUT_WR);
#endif
}

inline bool recv_line(SocketHandle sock, string& line) {
    line.clear();
    char ch;
    while(true) {
#ifdef _WIN32
        int got = recv(sock, &ch, 1, 0);
#else
        ssize_t got = recv(sock, &ch, 1, 0);
#endif
        if(got <= 0) return false;
        if(ch == '\n') break;
        if(ch != '\r') line.push_back(ch);
    }
    return true;
}

inline SocketHandle create_server_socket(int port) {
    SocketHandle server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == INVALID_SOCKET_HANDLE) {
        throw runtime_error("无法创建服务器 socket");
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if(bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket(server_fd);
        throw runtime_error("端口绑定失败，请检查端口是否被占用");
    }

    if(listen(server_fd, 8) < 0) {
        close_socket(server_fd);
        throw runtime_error("监听端口失败");
    }

    return server_fd;
}

inline SocketHandle accept_client(SocketHandle server_fd) {
    sockaddr_in client_addr{};
#ifdef _WIN32
    int len = sizeof(client_addr);
#else
    socklen_t len = sizeof(client_addr);
#endif
    return accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &len);
}

inline SocketHandle connect_to_server(const string& host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    string port_text = to_string(port);
    if(getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0) {
        throw runtime_error("无法解析服务器地址");
    }

    SocketHandle sock = INVALID_SOCKET_HANDLE;
    for(addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if(sock == INVALID_SOCKET_HANDLE) continue;
        if(connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
            break;
        }
        close_socket(sock);
        sock = INVALID_SOCKET_HANDLE;
    }

    freeaddrinfo(result);
    if(sock == INVALID_SOCKET_HANDLE) {
        throw runtime_error("无法连接到服务器");
    }
    return sock;
}

class Endpoint {
public:
    virtual ~Endpoint() = default;
    virtual void write(const string& text) = 0;
    virtual bool read_line(string& line) = 0;
};

class ConsoleEndpoint : public Endpoint {
public:
    void write(const string& text) override {
        cout << text;
        cout.flush();
    }

    bool read_line(string& line) override {
        return static_cast<bool>(getline(cin, line));
    }
};

class SocketEndpoint : public Endpoint {
private:
    SocketHandle sock;

public:
    explicit SocketEndpoint(SocketHandle s) : sock(s) {}

    ~SocketEndpoint() override {
        if(sock != INVALID_SOCKET_HANDLE) close_socket(sock);
    }

    void write(const string& text) override {
        send_all(sock, text);
    }

    bool read_line(string& line) override {
        return recv_line(sock, line);
    }
};

inline void run_client(const string& host, int port) {
    NetworkRuntime runtime;
    SocketHandle sock = connect_to_server(host, port);
    cout << "已连接到 " << host << ":" << port << "，等待房主开始游戏...\n";

    atomic<bool> running(true);
    thread reader([&]() {
        char buffer[1024];
        while(running) {
#ifdef _WIN32
            int got = recv(sock, buffer, sizeof(buffer), 0);
#else
            ssize_t got = recv(sock, buffer, sizeof(buffer), 0);
#endif
            if(got <= 0) break;
            cout.write(buffer, got);
            cout.flush();
        }
        running = false;
    });

    string line;
    while(running && getline(cin, line)) {
        if(!send_all(sock, line + "\n")) break;
    }

    shutdown_socket_write(sock);
    if(reader.joinable()) reader.join();
    running = false;
    close_socket(sock);
}

#endif
