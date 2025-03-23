#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

#define MAX_EVENTS 10
#define PORT 8080
#define BUFFER_SIZE 1024

void set_nonblocking(const int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // 创建监听 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // 设置 socket 为非阻塞
    set_nonblocking(server_fd);

    // 配置服务器地址
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 绑定和监听
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }
    if (listen(server_fd, SOMAXCONN) < 0) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;

    // 创建 epoll 实例
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "Epoll creation failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // 注册 server_fd 到 epoll
    struct epoll_event event{};
    event.events = EPOLLIN; // 监听可读事件
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
        std::cerr << "Epoll_ctl failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // 事件循环
    char buffer[BUFFER_SIZE];
    while (true) {
        struct epoll_event events[MAX_EVENTS];
        int fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (fds < 0) {
            std::cerr << "Epoll_wait failed: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < fds; i++) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                // 处理新连接
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd < 0) {
                    std::cerr << "Accept failed: " << strerror(errno) << std::endl;
                    continue;
                }
                set_nonblocking(client_fd);
                event.events = EPOLLIN | EPOLLET; // 边缘触发
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
                    std::cerr << "Epoll_ctl failed for client: " << strerror(errno) << std::endl;
                    close(client_fd);
                    continue;
                }
                std::cout << "New client connected: " << client_fd << std::endl;
            } else {
                // 处理客户端数据
                while (true) {
                    ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);
                    if (bytes_read < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 数据读完（ET模式）
                        }
                        std::cerr << "Read error: " << strerror(errno) << std::endl;
                        close(fd);
                        break;
                    }
                    if (bytes_read == 0) {
                        // 客户端关闭连接
                        std::cout << "Client " << fd << " disconnected" << std::endl;
                        close(fd);
                        break;
                    }
                    buffer[bytes_read] = '\0';
                    std::cout << "Received: " << buffer << std::endl;
                    write(fd, buffer, bytes_read); // 回显
                }
            }
        }
    }

    // 清理
    close(server_fd);
    close(epoll_fd);
    return 0;
}