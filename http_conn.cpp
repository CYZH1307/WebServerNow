#include "http_conn.h"

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

//设置文件描述符非阻塞
void setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_flag | O_NONBLOCK);
}

//在epoll中添加监听的文件描述符
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    // event.events = EPOLLIN | EPOLLRDHUP;
    event.events = EPOLLIN | EPOLLET |EPOLLRDHUP;

    if(one_shot) {
        event.events | EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd); 
}
//在epoll中移除监听的文件描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}
//在epoll中修改监听的文件描述符
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
void http_conn::process() { //处理请求的入口函数
    //解析http请求
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return ;
    }

    //生成响应
    bool write_ret = process_write( read_ret );
    if ( !write_ret ) {
        close_conn();
    }
    modfd( m_epollfd, m_sockfd, EPOLLOUT);
}

void http_conn::init(int sockfd, const sockaddr_in& addr) {
    m_sockfd = sockfd;
    m_address = addr;

    //端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    addfd(m_epollfd, m_sockfd, true);
    m_user_count ++; // 用户数量

    init();
}

void http_conn::init() {
    m_checked_state = CHECK_STATE_REQUESTLINE; //初始状态
    m_checked_idx = 0;
    m_start_line = 0;
    m_read_idx = 0;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_linger = false;

    bzero(m_read_buf, READ_BUFFER_SIZE);
}
void http_conn::close_conn() {
    if(m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count --;

    }
}
//读数据
bool http_conn::read() { 
    if(m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }

    int bytes_read = 0;
    while(true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) { //没有数据了
                break;
            }
            return false;
        } else if(bytes_read == 0) { //对方关闭连接
            return false;
        }
        m_read_idx += bytes_read;
    }   
    printf("读取数据：%s\n", m_read_buf);

    return true;
}

//写数据
bool http_conn::write() {

    
    return true;
}

http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char* text = 0;
    
    while(((line_status = parse_line) == LINE_OK) || (m_checked_state == CHECK_STATE_CONTENT && line_status == LINE_OK)) {
        text = get_line();

        m_start_line = m_checked_idx;
        printf("got http line: %s\n", text);

        switch(m_checked_state) {
            case CHECK_STATE_REQUESTLINE : {
                ret = parse_request_line(text);

                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER : {
                ret parse_headers(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST) {
                    return do_request();
                }
            }
            case CHECK_STATE_CONTENT : {
                ret = parse_content(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST) {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            defualt : {
                return INTERNAL_ERROR;
            }
        }

        return NO_REQUEST;
    }

}


http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    m_url = strpbrk(text, " \t");

    *m_url++ = '\0';

    char* method = text;
    if(strcasecmp(method, "GET") == 0) {
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }

    m_version = strpbrk(m_url, " \t");
    if(!m_version) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';

    if(strcasecmp(method, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    } 
    if(strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;  
        m_url = strchr(m_url, '/');
    }
    if(!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }

    m_checked_state = CHECK_STATE_HEADER;
}
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
    if( text[0] == '\0' ) {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if ( m_content_length != 0 ) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } else if ( strncasecmp( text, "Connection:", 11 ) == 0 ) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 ) {
            m_linger = true;
        }
    } else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 ) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol(text);
    } else if ( strncasecmp( text, "Host:", 5 ) == 0 ) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    } else {
        printf( "oop! unknow header %s\n", text );
    }
    return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    if ( m_read_idx >= ( m_content_length + m_checked_idx ) )
    {
        text[ m_content_length ] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for(; m_checked_idx < m_read_idx; m_checked_idx ++) {
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r') {
            if(m_checked_idx + 1 == m_read_idx) {
                return LINE_OPEN;
            } 
            else if(m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx ++] == '\0';
                m_read_buf[m_checked_idx ++] == '\0';

                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n') {
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx ++] = '\0';

                return LINE_OK;
            }
            return LINE_BAD;
        }

        // return LINE_OPEN; // ?
    }

    return LINE_OK;
}

http_conn::LINE_STATUS http_conn::do_request() {
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        return BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );
    // 创建内存映射
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return FILE_REQUEST;
} /////////