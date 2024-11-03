#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

class ThreadPool{
public:
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    explicit ThreadPool(int threadCount = 10): m_pool(std::make_shared<Pool>()){
        assert(threadCount>0);
        for(int i=0;i<threadCount;i++){
            std::thread([this](){
                std::unique_lock<std::mutex> locker(m_pool->mtx);
                while(true){
                    if(!m_pool->tasks.empty()){
                        auto task = m_pool->tasks.front();
                        m_pool->tasks.pop();
                        locker.unlock();   // 元素取出来了，提前解锁
                        if(task){
                            task(); // 取出来的是个匿名函数，直接执行
                            std::cout<<"task start"<<std::endl;
                        }  
                        locker.lock();  // 继续取任务，所以要上锁
                    }
                    else if(m_pool->isClosed){
                        break;   // 停止了直接跳出
                    }
                    else{
                        m_pool->conv.wait(locker);   // 没任务就在这等着，等着条件变量唤起
                    }
            }
            }).detach(); // 添加线程
        }
    }
    ~ThreadPool(){
        std::cout<< "~Threadpool"<<std::endl;
        if(m_pool){
            std::unique_lock<std::mutex> locker(m_pool->mtx);
            m_pool->isClosed = true;
        }
        m_pool->conv.notify_all();  // 通知所有的线程去消费掉
        std::cout<<"~ThreadPool end"<<std::endl;
    }

    template<typename T>
    void addTask(T&& task){
        std::unique_lock<std::mutex> locker(m_pool->mtx);
        m_pool->tasks.emplace(std::forward<T>(task));
        std::cout<<"加入线程池"<<std::endl;
        m_pool->conv.notify_one(); // 添加了任务就通知一个线程去消费，可以并发，因为只有取任务的时候是锁被用的，执行的时候是各个线程里面执行
    }
private:
    struct Pool{
        std::mutex mtx;
        std::queue<std::function<void()>> tasks;  // 任务队列
        bool isClosed = false;
        std::condition_variable conv;


    };
    std::shared_ptr<Pool> m_pool;
};

#endif //THREADPOOL_H