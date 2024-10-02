#ifndef LOG_H
#define LOG_H

#include "blockqueue.h"
#include "../buffer/buffer.h"
#include <thread>
#include <mutex>
#include <assert.h>
#include <sys/time.h>
#include <stdarg.h>

class Log {
public:
    void init(int level, const char* path = "./log", 
            const char* suffix = ".log", 
            bool isPrintConsole = true,
            int maxQueueCapacity = 1024);
    // 单例模式
    static Log* instance();
    static void flushLogThread();

    // 异步日志写入buffer
    void write(int level,const char* logline,...);  // 处理多个参数的日志，记录日志
    void flush();

    int getLevel();
    void setLevel(int level);
    int getLine(){
        return m_count;
    }
private:
    Log();
    virtual ~Log();
    void appendLogLevelTitle(int level);  // 添加日志等级标题
    void asyncWrite(); // 异步写入私有日志


private:
    static const int LOG_PATH_LEN = 256;  // 日志路径长度
    static const int LOG_NAME_LEN = 256;  // 日志名字长度
    static const int MAX_LINES = 50000;  // 日志最大行数

    const char* m_path;  // 日志路径
    const char* m_suffix;  // 日志后缀

    std::atomic<int> m_count;  // 当前行数,使用原子变量
    int fileCount;
    int m_today;  // 当前日期
    int m_level;  // 日志等级

    Buffer m_buffer;  // 缓冲区
    bool m_isAsync;  // 是否异步
    bool m_isPrintConsole;  // 是否打印到控制台


    std::unique_ptr<BlockQueue<std::string>> m_logQueue;  // 阻塞队列
    std::unique_ptr<std::thread> m_writeThread;  // 写日志线程指针
    std::mutex m_mutex;  // 互斥锁
    FILE* m_fp;  // 打开log文件的文件指针
};

// 宏函数，调用过程
// ##__VA_ARGS__表示可变参数,如果没有参数，会去掉前面的逗号，防止编译错误
// flush是将日志写入文件
// 日志都是先写入 buffer，然后再写入阻塞队列，最后再写入文件
#define LOG_BASE(level, format, ...)\
    do{\
        Log* log = Log::instance();\
        if (log->getLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__);\
            log->flush();\
        }\
    }while(0)
// __FILE__, __LINE__,

// 只有小于实际日志等级的日志才会输出，写入文件，所以 DEBUG < INFO < WARN < ERROR
#define LOG_DEBUG(format, ...) LOG_BASE(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) LOG_BASE(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) LOG_BASE(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LOG_BASE(3, format, ##__VA_ARGS__)
#endif // LOG_H