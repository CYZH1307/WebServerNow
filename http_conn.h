#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include <string>

#include "locker.h"

class http_conn {
public:
    static int m_epollfd;
    static int m_user_count;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 2048;

    // HTTP请求方法，这里只支持GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    // 请求行，首部行，实体体
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    // 读取到一个完整的行，行出错，行数据尚且不完整
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { NO_REQUEST = 0, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };

    http_conn() {};
    ~http_conn() {};

    void process(); //处理客户端请求
    void init(int sockfd, const sockaddr_in& addr); //初始化新连接
    void close_conn(); //关闭连接
    bool read(); //非阻塞读
    bool write(); //非阻塞写



private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_idx; //读到的位置
    int m_checked_idx; //解析到的位置
    int m_start_line; //解析行的起始位置
    char* m_url; //请求url
    char* m_version; //请求版本
    METHOD m_method; // 请求方法
    char* m_host; //主机名
    bool m_linger; //保持连接

    CHECK_STATE  m_checked_state; //主状态机状态

    void init(); //初始化连接其余数据

    HTTP_CODE process_read(); //解析http请求
    HTTP_CODE parse_request_line(char* text); //解析请求行
    HTTP_CODE parse_headers(char* text); //解析首部行
    HTTP_CODE parse_content(char* text); //解析实体体
    LINE_STATUS parse_line();
    HTTP_CODE do_request();

    char* get_line() { return m_read_buf + m_start_line; }
};

#endif