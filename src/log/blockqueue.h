#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/time.h>  // 打印日志时间

template<typename T>
class BlockQueue {
public: 
    BlockQueue(size_t maxsize = 1000);
    ~BlockQueue();

    // 向队列中添加/取元素
    void put(const T& value);

    bool take(T& value);  // 将take出来的元素放到value中

    bool empty() const;
    bool full() const;
    size_t size() const;

    void flush();
    void close();
    void clear();

private:
    mutable std::mutex m_mutex;  // 互斥锁
    std::condition_variable m_notEmpty;  // 消费者条件变量，没空就可以消费
    std::condition_variable m_notFull;  // 生产者条件变量，没满就可以生产
    std::deque<T> m_deque;  // 队列
    size_t m_maxSize;  // 队列最大长度
    bool m_isClosed;  // 队列是否关闭
};

template<typename T>
BlockQueue<T>::BlockQueue(size_t maxsize) : m_maxSize(maxsize) {
    m_isClosed = false;  /// 队列未关闭
};

template<typename T>
BlockQueue<T>::~BlockQueue() {
    close();
    std::cout<<" BlockQueue destory..."<<std::endl;
    m_isClosed = true;
};

template<typename T>
void BlockQueue<T>::put(const T& value) {
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_deque.size() >= m_maxSize) {
        m_notFull.wait(lock);
    }
    m_deque.push_back(value);
    // std::cout<<"put success.m_deque size = "<<m_deque.size()<<std::endl;
    m_notEmpty.notify_one();
}


template<typename T>
bool BlockQueue<T>::take(T& value) {
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::cout<<"size="<<m_deque.size()<<std::endl;
    while(m_deque.empty()){
        // std::cout<<"卡在take函数"<<std::endl;
        m_notEmpty.wait(lock);
    }
    value = m_deque.front();
    m_deque.pop_front();
    m_notFull.notify_one();
    return true;
}

template<typename T>
bool BlockQueue<T>::empty() const {
    std::lock_guard<std::mutex> lock(m_mutex);  // 操控队列之前都需要上锁
    return m_deque.empty();
}

template<typename T>
bool BlockQueue<T>::full() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_deque.size() == m_maxSize;
}

template<typename T>
size_t BlockQueue<T>::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_deque.size();
}

template<typename T>
void BlockQueue<T>::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_notEmpty.notify_all();    // 通知消费者消费掉剩余的数据
}

template<typename T>
void BlockQueue<T>::close() {
    clear();
    m_isClosed = true;
    m_notFull.notify_all();  // 通知生产者队列已关闭
    m_notEmpty.notify_all();  // 通知消费者队列已关闭 
    std::cout<<"BlockQueue closed"<<std::endl;
}

template<typename T>
void BlockQueue<T>::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deque.clear(); // 清除队列
}

#endif // BLOCKQUEUE_H
