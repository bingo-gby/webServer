#include "epoller.h"

Epoller::Epoller(int maxEvent) : 
    epollFd_(epoll_create1(EPOLL_CLOEXEC)), 
    events_(maxEvent) {
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::addFd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;  // events是个位掩码，可以是EPOLLIN, EPOLLOUT等
    // 添加新的事件到epoll中，但是不会立即激活，就是没有操作事件数组
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);  
}

bool Epoller::modFd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    // 修改事件，如果fd不在epoll中，会返回错误
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);  // 修改事件
}

bool Epoller::delFd(int fd) {
    // 删除事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, 0);  
}

int Epoller::wait(int timeoutMs) {
    // 等待事件
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);  
}

// 获取第i个事件的fd
int Epoller::getEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

// 获取第i个事件的事件类型
uint32_t Epoller::getEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}