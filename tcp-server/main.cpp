#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>

class TcpServer
{
public:
    TcpServer()
    {
        // 创建服务器socket
        _server_fd = socket(AF_INET, SOCK_STREAM, 0);
        set_nonblocking(_server_fd);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORT);
        bind(_server_fd, (sockaddr*)&addr, sizeof(addr));
        listen(_server_fd, SOMAXCONN);

        // 创建epoll实例
        _epoll_fd = epoll_create1(0);
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = _server_fd;
        epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server_fd, &ev);
    }

    ~TcpServer()
    {
        close(_server_fd);
        close(_epoll_fd);
    }

    void run()
    {
        epoll_event events[MAX_EVENTS];
        std::cout << "Server running on port " << PORT << "\n";
        while (true)
        {
            int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);
            for (int i = 0; i < nfds; ++i)
            {
                if (events[i].data.fd == _server_fd)
                {
                    // 新连接
                    int client_fd = accept(_server_fd, nullptr, nullptr);
                    set_nonblocking(client_fd);
                    epoll_event ev{};
                    ev.events = EPOLLIN | EPOLLET; // 边缘触发
                    ev.data.fd = client_fd;
                    epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                }
                else
                {
                    // 处理客户端数据
                    char buf[1024]{};
                    int fd = events[i].data.fd;
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n <= 0)
                    {
                        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                        close(fd);
                    }
                    else
                    {
                        write(fd, buf, n);
                    }
                }
            }
        }
    }
private:
    static void set_nonblocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    int _server_fd;
    int _epoll_fd;
    static constexpr int MAX_EVENTS = 64;
    static constexpr int PORT = 8080;
};


int main()
{
    TcpServer server;
    server.run();
    return 0;
}