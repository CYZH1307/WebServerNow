#include <iostream>
#include "webserver.h"
#include <cstring>

#include <winsock.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// #include

int main(int argc, char* argv[]) {
    std::cout << "WebServer Now!\n";

    SOCKET socket_fd = socket(AF_INET, SOCK_STREAM, 0);//IP类型，传输方式，协议

    if(socket_fd == INVALID_SOCKET) {
        std::cout<< "server_socket = INVALID_SOCKET\n";
        return 1;
    }
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    // 设置地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("114.115.151.138");
    server_addr.sin_port = htons(8888);

    bind(socket_fd, (sockaddr*)server_addr, sizeof(server_addr));

    listen(socket_fa, SOMAXCONN);

    {
        struct sockaddr_in clent_addr;
        socklen_t clent_addr_len = sizeof(clent_addr);
        bzero(&clent_addr, sizeof(clent_addr));

        int clent_socket_fd = accept(socket_fd, (sockaddr*)(clent_addr), &clent_addr_len);
        printf("new client fd %d! IP: %s Port: %d\n", clnt_sockfd, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
    }

    return 0;
}
