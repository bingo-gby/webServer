#ifndef HTTPCONN_H
#define HTTPCONN_H
// Httpconn作用：读写数据，调用httpresponse生成响应报文，调用httprequest解析请求报文

#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "httpRequest.h"
#include "httpResponse.h"

/*
进行读写数据并调用httprequest 来解析数据以及httpresponse来生成响应
*/

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);
    ssize_t read(int* saveErrno); // saveErrno 用于保存错误码
    ssize_t write(int* saveErrno);
    void httpclose(); // 避免跟close函数重名
    int getFd() const;
    int getPort() const;  // 获取端口
    const char* getIP() const;
    sockaddr_in getAddr() const;
    bool process(); // 处理请求

    int ToWriteBytes() { return m_iov[0].iov_len + m_iov[1].iov_len; }
    
    bool isKeepAlive() const {
        return m_request.isKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount; // 原子操作，用于统计用户数量

private:
    int m_sockFd;
    sockaddr_in m_addr;
    bool m_isClose;

    int m_iovCnt;
    struct iovec m_iov[2];

    Buffer m_readBuff;  // 读缓冲区，从·socket读 request，存到这里
    Buffer m_writeBuff; // 写缓冲区，生成的 response 写到这里

    HttpRequest m_request; // 解析请求
    HttpResponse m_response; // 生成响应
};



#endif //HTTPCONN_H