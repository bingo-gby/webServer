#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> // epoll_create1, epoll_ctl, epoll_wait
#include <unistd.h> // close
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
public:
    // maxEvent: epoll_wait()最多监听的事件数
    Epoller(int maxEvent = 1024);   
    ~Epoller();
    // 添加事件
    bool addFd(int fd, uint32_t events); 
    // 修改事件
    bool modFd(int fd, uint32_t events); 
    // 删除事件
    bool delFd(int fd);  
    // 等待事件
    int wait(int timeoutMs = -1);  
 
    // 获取第i个事件的fd
    int getEventFd(size_t i) const;  
    // 获取第i个事件的事件类型
    uint32_t getEvents(size_t i) const; 

private:
    // epoll句柄
    int epollFd_; 
    // 事件数组
    std::vector<struct epoll_event> events_; 
};

#endif // EPOLLER_H