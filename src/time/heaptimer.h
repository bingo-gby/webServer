#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;
    TimeStamp expires;  // 超时时间
    TimeoutCallBack cb;  // 超时回调函数
    // 重载<运算符
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
    // 重载>运算符
    bool operator>(const TimerNode& t) {
        return expires > t.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() { heap.reserve(64); }  // 预留空间
    ~HeapTimer() { clear(); }

    // 调整指定 id 的超时时间
    void adjust(int id, int newExpires); 
    // 添加一个定时器
    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);
    void clear();
    void tick();
    void pop();
    int getNextTick();

private:
    void del_(size_t i);
    void siftup_(size_t i);
    bool siftdown_(size_t index, size_t n);
    void SwapNode_(size_t i, size_t j); // 交换两个索引的节点

    std::vector<TimerNode> heap;  // heap其实就是一个vector
    std::unordered_map<int, size_t> ref; // {id：index}的 unordered_map
};
#endif //HEAPTIMER_H