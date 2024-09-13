#include "buffer.h"


Buffer::Buffer(size_t buffersize) : m_buffer(buffersize), m_readPos(0), m_writePos(0) {}
Buffer::~Buffer() {}
// 读写指针操作
size_t Buffer::writeableBytes() const {
    return m_buffer.size() - m_writePos;
}

size_t Buffer::readableBytes() const {
    return m_writePos - m_readPos;
}

size_t Buffer::prependableBytes() const {
    return m_readPos;
}

// 写入数据函数
void Buffer::append(const std::string& str) {
    append(str.data(), str.size());  // str.data()返回指向字符串首字符的指针,跟c_str()类似
}

void Buffer::append(const char* data, size_t len) {
    ensureWriteableBytes(len);
    std::copy(data, data + len, beginWrite());
    m_writePos += len;
}

// 读取数据函数
void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        m_readPos += len;
    } else {
        retrieveAll();   // 读取所有数据，清空缓存区
    }
}

void Buffer::retrieveUntil(const char* end) {
    retrieve(end - peek());
}

void Buffer::retrieveAll() {
    m_readPos = 0;
    m_writePos = 0;
}

std::string Buffer::retrieveToStr(size_t len) {
    std::string str(peek(), len);
    retrieve(len);  // 移动读取指针
    // std::cout<< "str:"<<str<<std::endl;
    return str;
}

std::string Buffer::retrieveAllToStr() {
    // std::cout<<"開始移動所有可讀字節"<<std::endl;
    return retrieveToStr(readableBytes());
}

// 预留区域操作
void Buffer::prepend(const void* data, size_t len) {
    if (len > prependableBytes()) {
        std::cout << "len > prependableBytes()" << std::endl;
        return;
    }
    m_readPos -= len;  // 先移动读取指针，然后再copy过去
    const char* d = static_cast<const char*>(data);
    std::copy(d, d + len, m_buffer.begin() + m_readPos);
}

// 数据扩展操作
void Buffer::ensureWriteableBytes(size_t len) {
    if (writeableBytes() < len) {
        makeSpace(len);
    }
}
void Buffer::makeSpace(size_t len) {
    if (writeableBytes() + prependableBytes() < len) {
        m_buffer.resize(m_writePos + len);
    } else {
        size_t readable = readableBytes();
        std::copy(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos, m_buffer.begin());
        m_readPos = 0;
        m_writePos = m_readPos + readable;
    }
}

// 底层数据访问
const char* Buffer::peek() const {
    return &m_buffer[m_readPos];
}

char* Buffer::beginWrite() {
    return &m_buffer[m_writePos];
}

void Buffer::hasWritten(size_t len) {
    m_writePos += len;
}

// fd操作
ssize_t Buffer::readFd(int fd, int* saveErrno) {
    char stackbuf[65536];
    struct iovec vec[2];
    const size_t writable = writeableBytes();
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = stackbuf;
    vec[1].iov_len = sizeof(stackbuf);
    const ssize_t n = readv(fd, vec, 2); //  n为从fd读取的字节数
    if (n < 0) {
        *saveErrno = errno;     // 返回负值，读取错误
    } else if (static_cast<size_t>(n) <= writable) {
        m_writePos += n;   // 读取成功，移动写入指针
    } else {
        m_writePos = m_buffer.size();  // 已经写满
        append(stackbuf, n - writable); // 将剩余数据写入,这里会扩容
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno) {
    size_t readSize = readableBytes();
    ssize_t n = write(fd, peek(), readSize);  // 从缓冲区写数据到fd
    if (n < 0) {
        *saveErrno = errno;
    }
    m_readPos += n;  // 移动读取指针
    return n;
}