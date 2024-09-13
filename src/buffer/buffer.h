#ifndef BUFFER_H  // 防止重复定义
#define BUFFER_H
// buffer模块的头文件
#include <iostream>
#include <vector>
#include <atomic>
#include <unistd.h> // 包含系统调用的头文件
#include <sys/uio.h>  // 包含readv和writev函数的头文件


class Buffer {

public:
    // 构造函数和析构函数
    Buffer(size_t buffersize = 1024);
    ~Buffer();

    // 读写指针操作
    size_t writeableBytes() const;  // 返回可写字节数
    size_t readableBytes() const;  // 返回可读字节数
    size_t prependableBytes() const;  // 返回预留字节数

    // 写入数据函数
    void append(const std::string& str);  // 写入字符串，缓存区不够大会自动扩容
    void append(const char* data, size_t len);  // 写入字符数组，如果缓存区不够大，会自动扩容，len为写入长度

    // 读取数据函数
    void retrieve(size_t len);  // 读取len长度的数据，只移动读取指针
    void retrieveUntil(const char* end);  // 读取到end位置，只移动读取指针
    void retrieveAll();  // 读取所有数据，清空缓存区，读写指针归零
    std::string retrieveToStr(size_t len);  // 读取len长度的数据，返回字符串,并移动读取指针
    std::string retrieveAllToStr();  // 读取所有数据，返回字符串，并清空缓存区

    // 预留区域操作
    void prepend(const void* data, size_t len);  // 将数据插入到缓冲区的预留区域prependable区域

    // 数据扩展操作
    void ensureWriteableBytes(size_t len);  // 确保缓冲区有足够的空间写入len长度的数据
    void makeSpace(size_t len);  // 扩容函数

    // 底层数据访问
    const char* peek() const;  // 返回读取指针位置，目的是读取数据
    char* beginWrite() ;  // 返回写入指针位置，目的是写入数据
    void hasWritten(size_t len);  // 移动写入指针

    // fd操作
    ssize_t readFd(int fd, int* saveErrno);  // 从fd中读取数据到缓冲区
    ssize_t writeFd(int fd, int* saveErrno);  // 从缓冲区写数据到fd

private:
    std::vector<char> m_buffer;  // 成员变量，使用m_作为前缀
    std::atomic<size_t> m_readPos;  // 读取位置, 使用原子变量保证线程安全，且使用size_t类型，无符号整型
    std::atomic<size_t> m_writePos;  // 写入位置
};
#endif // BUFFER_H