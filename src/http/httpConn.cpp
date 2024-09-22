#include <httpConn.h>

// 声明静态成员变量
const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET; 

HttpConn::HttpConn() {
    m_sockFd = -1;
    m_addr = {0};
    m_isClose = true;
}

HttpConn::~HttpConn() {
    httpclose();
}

// addr 是客户端的地址(套接字地址)
void HttpConn::init(int sockFd, const sockaddr_in& addr) {
    assert(sockFd > 0);
    userCount++;
    m_addr = addr;
    m_sockFd = sockFd;
    m_readBuff.retrieveAll();
    m_writeBuff.retrieveAll();
    m_iovCnt = 0;
    m_isClose = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", m_sockFd, getIP(), getPort(), (int)userCount);
}

void HttpConn::httpclose() {
    m_response.unmapFile();
    if(m_isClose == false) {
        m_isClose = true;
        userCount--;
        close(m_sockFd);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", m_sockFd, getIP(), getPort(), (int)userCount);
    }
}

int HttpConn::getFd() const {
    return m_sockFd;
}

sockaddr_in HttpConn::getAddr() const {
    return m_addr;
}

const char* HttpConn::getIP() const {
    return inet_ntoa(m_addr.sin_addr);
}

int HttpConn::getPort() const {
    return ntohs(m_addr.sin_port);
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = m_readBuff.readFd(m_sockFd, saveErrno);
        if(len <= 0) {
            break;
        }
    } while(isET);  // ET模式下，需要一次性将数据读完
    // ET就是文件描述符上有数据到来时，只会通知一次，直到将数据读完
    return len;
}

ssize_t HttpConn::write(int* saveErrno){
    ssize_t len = -1;
    do {
        len = writev(m_sockFd, m_iov, m_iovCnt); // 将数据写到socket
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        // == 0 说明数据已经全部写完
        if(m_iov[0].iov_len + m_iov[1].iov_len == 0) {
            break;
        } 
        // 写入的字节大于第一个缓冲区的长度，调整m_iov[1]基地址和长度，清空m_iov[0]
        else if(static_cast<size_t>(len) > m_iov[0].iov_len) {
            m_iov[1].iov_base = (uint8_t*)m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            if(m_iov[0].iov_len) {
                m_writeBuff.retrieveAll();
                m_iov[0].iov_len = 0;
            }
        } else {
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len;
            m_iov[0].iov_len -= len;
            m_writeBuff.retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240); // ET模式下，需要一次性将数据写完
    return len;
}

// 处理请求并生成响应
bool HttpConn::process(){
    m_request.init();
    if(m_readBuff.readableBytes() <= 0) {
        return false;
    } else if(m_request.parse(m_readBuff)) {
        LOG_DEBUG("%s", m_request.path().c_str());
        m_response.init(srcDir, m_request.path(), m_request.isKeepAlive(), 200);
    } else {
        m_response.init(srcDir, m_request.path(), false, 400);
    }
    m_response.makeResponse(m_writeBuff);
    // 响应头部信息
    m_iov[0].iov_base = const_cast<char*>(m_writeBuff.peek());
    m_iov[0].iov_len = m_writeBuff.readableBytes();
    m_iovCnt = 1;
    // 文件映射区
    if(m_response.fileLen() > 0 && m_response.file()) {
        m_iov[1].iov_base = m_response.file();   // m_iov[1]指向文件映射区
        m_iov[1].iov_len = m_response.fileLen();
        m_iovCnt = 2;
    }
    LOG_DEBUG("filesize:%d, %d to %d", m_response.fileLen(), m_iovCnt, ToWriteBytes());
    return true;
}

