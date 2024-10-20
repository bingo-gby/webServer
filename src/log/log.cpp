#include "log.h"
#include <sys/stat.h> // mkdir
#include <sys/syscall.h>

Log::Log() {
    m_fp = nullptr;
    m_isAsync = false;  // 默认同步
    m_writeThread = nullptr;
    m_logQueue = nullptr;
    m_count = 0;
    m_today = 0;
    fileCount =0;
}

Log::~Log() {
    std::cout<<"~log begin"<<std::endl;
    while(!m_logQueue->empty()) {
        m_logQueue->flush();
    }
    if(m_writeThread && m_writeThread->joinable()){
        std::cout<< " 开始join" <<std::endl;
        m_writeThread->join();
    }

    std::cout<< " 开始close m_logQueue" <<std::endl;
    m_logQueue->close();
    std::cout<< " 完成close m_logQueue" <<std::endl;

    if(m_fp) {
        std::unique_lock<std::mutex> locker(m_mutex);
        flush();
        fclose(m_fp);
    }
    std::cout<<"~log end"<<std::endl;
}
void Log::flush() {
    if(m_isAsync) {
        m_logQueue->flush();
    } else {
        fflush(m_fp);
    }
}

// 生成新变量，懒汉模式，局部静态变量，不需要加锁，但是生命周期是跟程序相同
Log* Log::instance() {
    static Log log;
    return &log;
}

// void printThreadTID() {
//     pid_t tid = syscall(SYS_gettid);
//     std::cout << "Thread TID: " << tid << std::endl;
// }
// 异步日志的写线程函数
void Log::flushLogThread() {
    // printThreadTID();
    std::cout<<"writeThread start..."<<std::endl;
    Log::instance()->asyncWrite();
    std::cout<<"writeThread end..."<<std::endl;
}

void Log::asyncWrite() {
    std::string str = "";
    // std::cout<<"开始写入前m_logQueue->size ="<<m_logQueue->size()<<std::endl;
    while(m_logQueue->take(str)) {
        // std::unique_lock<std::mutex> locker(m_mutex);
        // {
        //     std::unique_lock<std::mutex> locker(m_mutex);
        //     std::cout<< "size="<<m_logQueue->size()<<std::endl;
        // }
        // std::cout<<"str="<<str<<std::endl;
        if((fputs(str.c_str(), m_fp))== EOF){
            std::cout<<"写入 m_fp 失败" <<std::endl;
        }
        else{
            // std::cout<<"写入成功"<<std::endl;
        }
        fflush(m_fp); // 刷新缓存区

    }
    // std::cout<<"2222222:"<<m_logQueue->size()<<std::endl;
}

void Log::init(int level, const char* path, const char* suffix, bool isPrintConsole, int maxQueueCapacity) {
#ifdef _DEBUG
    std::cout<<string(path)<std::endl;
    std::cout<<string(suffix)<std::endl;
#endif
    m_isPrintConsole = isPrintConsole;
    m_level = level;
    m_path = path;
    m_suffix = suffix;

    // 说明是异步模式
    if(maxQueueCapacity > 0) {
        m_isAsync = true;
        if(!m_logQueue) {
            m_logQueue = std::make_unique<BlockQueue<std::string>>();
            std::unique_ptr<std::thread> writeThread(new std::thread(flushLogThread));
            m_writeThread = std::move(writeThread); // 写日志的线程
            if(!m_writeThread){
                std::cout << "failed to create log flush Thread"<<std::endl;
            }
        }
    }
    else{
        m_isAsync = false;
    }

    if(!m_logQueue){
        std::cerr<<"m_logQueue is null...";
    }
    m_count = 0;
    time_t timer = time(nullptr);
    struct tm* sysTime = localtime(&timer);
    struct tm t = *sysTime;
    m_today = t.tm_mday;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN-1, "%s/%04d_%02d_%02d%s", path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix);   // 储存在 filename 中，这是一个文件名
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        m_buffer.retrieveAll();  // 清空 buffer，取出所有数据
        m_count = 0;
        // 如果已经打开了文件，先关闭文件
        if(m_fp) {
            flush();
            fclose(m_fp);
        }
        m_fp = fopen(fileName, "a");  // 写入模式
        if(m_fp == nullptr) {
            mkdir(path, 0777);
            m_fp = fopen(fileName, "a");  // 生成文件
        }
        assert(m_fp != nullptr);
    }

    std::cout<< " Init Success"<<std::endl;
}

// 记录日志  const char* file,int line,
void Log::write(int level,const char* format, ...) {
    // std::cout<<" start write"<<std::endl;
    // 将系统时间转为本地时间
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm* sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    // 获取线程id
    // std::string thread_id_str = std::to_string(pthread_self());

    // 如果不是今天，或者写入的行数达到了最大行数，按天数分和按行数分，避免每个日志文件太大
    // std::cout<<"today:"<<m_today<<"  tm_mday:"<<t.tm_mday<<std::endl;
    // std::unique_lock<std::mutex> locker(m_mutex);
    if(m_today!= t.tm_mday  || (m_count && (m_count >= (fileCount +1)*MAX_LINES ))) {
        fileCount++;
        char newFile[LOG_NAME_LEN];  // 新文件名
        char tail[36] = {0};  // 时间戳
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        
        // 时间不匹配
        if(m_today != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN-72, "%s/%s%s", m_path, tail, m_suffix);
            m_today = t.tm_mday;
            m_count = 0;
            std::cout<<"时间不一致了，m_today="<<m_today<<"  t.tm_mday="<<t.tm_mday<<std::endl;
        } else {
            std::cout<<"m_count="<<m_count<<std::endl;
            snprintf(newFile, LOG_NAME_LEN-72, "%s/%s-%d%s", m_path, tail, fileCount, m_suffix);
            // std::cout<<"<<<<<"<<m_count / MAX_LINES<<std::endl;
        }
        std::unique_lock<std::mutex> locker(m_mutex);
        flush();
        // 关闭文件，避免 m_fp为空时 crash
        if(m_fp){
            fclose(m_fp);
        }
        #ifdef _DEBUG
            std::cout<<newFile<<std::endl;
        #endif
        m_fp = fopen(newFile, "a");  // 重新打开文件
        std::cout<<newFile<<std::endl;
        assert(m_fp != nullptr);
    }
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        m_count++;
        // int n = snprintf(m_buffer.beginWrite(), 128, 
        //                 "%04d-%02d-%02d %02d:%02d:%02d.%06ld [%s:%d] [Tid: %s]", 
        //                 t.tm_year + 1900, t.tm_mon + 1, 
        //                 t.tm_mday, t.tm_hour, 
        //                 t.tm_min, t.tm_sec, now.tv_usec,file,line,thread_id_str.c_str());
        // [%s:%d] [Tid: %s]
        // ,file,line,thread_id_str.c_str()
        int n = snprintf(m_buffer.beginWrite(), 128, 
                "%04d-%02d-%02d %02d:%02d:%02d.%06ld ", 
                t.tm_year + 1900, t.tm_mon + 1, 
                t.tm_mday, t.tm_hour, 
                t.tm_min, t.tm_sec, now.tv_usec);
        m_buffer.hasWritten(n);
        appendLogLevelTitle(level);

        va_start(vaList, format);  // 初始化vaList,准备读取可变参数，format为定位符
        int m = vsnprintf(m_buffer.beginWrite(), m_buffer.writeableBytes(), format, vaList);
        va_end(vaList);  // 清空vaList，完成可变参数的读取
        m_buffer.hasWritten(m);
        m_buffer.append("\n\0", 2);
        
        
        if(m_isAsync && m_logQueue && !m_logQueue->full()) {
            // std::cout<< "可读前:"<<m_buffer.readableBytes()<<std::endl;
            // std::cout<< "buffer移动前是:"<<m_buffer.retrieveAllToStr()<<std::endl;
            m_logQueue->put(m_buffer.retrieveAllToStr());   // 异步加入阻塞队列
            // std::cout<< "放完后,m_logQueue size=:"<<m_logQueue->size()<<std::endl;
        } else {
            fputs(m_buffer.peek(), m_fp);  // 同步写入文件
            fflush(m_fp);
            if(m_isPrintConsole) {
                if (fputs(m_buffer.peek(), stdout) == EOF) {
                    fflush(stdout);
                    std::cerr << "Failed to print to console!" << std::endl;
                }
            }
        }
        m_buffer.retrieveAll();  // 清空 buffer
    }
    // std::cout<< "可读:"<<m_buffer.readableBytes()<<std::endl;
    // std::cout<< "可写:"<<m_buffer.writeableBytes()<<std::endl;
    // std::cout<< "预留:"<<m_buffer.prependableBytes()<<std::endl;
    // std::cout<< " write finished" <<std::endl;
    // std::cout<<m_logQueue->size()<<std::endl;
}

void Log::appendLogLevelTitle(int level) {
    switch(level) {
        case 0:
            m_buffer.append("[debug]: ", 9);
            break;
        case 1:
            m_buffer.append("[info] : ", 9);
            break;
        case 2:
            m_buffer.append("[warn] : ", 9);
            break;
        case 3:
            m_buffer.append("[error]: ", 9);
            break;
        default:
            m_buffer.append("[info] : ", 9);
            break;
    }
}

int Log::getLevel() {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_level;
}

void Log::setLevel(int level) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_level = level;
}

