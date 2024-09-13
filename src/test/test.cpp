#include "../log/log.h"
#include "../pool/threadpool.h"

// level=3时，当 i>=3 才被记录，所以level为3时， 3 输出10次
// level=2时,  2 3各10次，依次类推 debug 10次， info 20次 warn 30次 error 40次，总共100次
void testLog(){
    int cnt = 0, level = 0;
    Log::instance()->init(level, "./testlog1",".log");
    for(level = 3; level >= 0; level--) {
        Log::instance()->setLevel(level);  
        for(int j = 0; j < 10; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 111111111 %d ============= ", "Test",++cnt);
            }
        }
    }
}

void threadLogTask(int i, int cnt) {
    for(int j = 0; j < 10000; j++ ){
        LOG_BASE(i,"PID:[%04d]======= %05d ========= ", gettid(), cnt++);
    }
}

void testThreadPool() {
    Log::instance()->init(0, "./testThreadpool", ".log", 5000);
    ThreadPool threadpool(10);
    for(int i = 0; i < 18; i++) {
        threadpool.addTask(std::bind(threadLogTask, i % 4, i * 100));
        std::cout<<"加入了"<<i+1<<std::endl;
    }
    sleep(5);
    std::cout<<Log::instance()->getLine()<<std::endl;
    getchar(); // 输入之后才会往下走
}

int main(){
    // testLog();
    testThreadPool();
    std::cout<<"main函数结束"<<std::endl;
    return 0;
}