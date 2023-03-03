#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <signal.h>

#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"


#define MAX_FD 65535 //最大文件描述符个数
#define MAX_EVENT_NUMBER 100000 //最大监听事件数量

extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);
extern void modfd(int epollfd, int fd, int ev);

//添加信号
void addsig(int sig, void(headler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = headler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

int main(int argc, char* argv[]) {
    if(argc <= 1) {
        printf("运行格式：%s port_number\n", basename(argv[0]));
        exit(-1);
    }

    int port = atoi(argv[1]); //获取端口号

    addsig(SIGPIPE, SIG_IGN);

    // 创建线程池，处理http连接任务
    threadpool<http_conn> * pool = NULL;
    try {
        pool = new threadpool<http_conn>;

    } catch(...) {
        printf("create threadpool<http_conn> fail\n");
        exit(-1);
    }

    http_conn* users = new http_conn[MAX_FD];

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    // 端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    //监听
    listen(listenfd, 5);
    
    // 创建epoll对象
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);

    // 添加文件描述符到epoll中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while(true) {
        // 等待事件触发
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if(num < 0 && errno != EINTR) {
            printf("epoll failure\n");
            break;
        }

        // 遍历事件
        for(int i = 0; i < num; i++) {
            int sockfd = events[i].data.fd;
            
            if(sockfd == listenfd) { //客户端连接
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);

                if(http_conn::m_user_count >= MAX_FD) { //连接满了
                    close(connfd);
                    continue;
                }

                users[connfd].init(connfd, client_address);
            } else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { //对方异常断开
                users[sockfd].close_conn(); //调用类的关闭连接函数
                
            } else if(events[i].events & EPOLLIN) {
                if(users[sockfd].read()) { //读完所有数据
                    pool->append(users + sockfd); //交给工作线程处理

                } else  {
                    users[sockfd].close_conn();
                } 
            } else if(events[i].events & EPOLLOUT) {
                if(!users[sockfd].write()) { //一次性写完所有数据
                    users[sockfd].close_conn();
                }
            }

        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;

    return 0;
}
