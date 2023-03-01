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


//添加信号
void addsig(int sig, void(headler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = headler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sam NULL);
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


    return 0;
}
