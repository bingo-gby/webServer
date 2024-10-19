#ifndef WEBSERVER_H
#define WEBSERVER_H
#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../time/heaptimer.h"

#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"

#include "../http/httpConn.h"

class webServer{
public:
    // 初始化
    webServer(int port, int trigMode, int timeoutMS, 
              bool OptLinger, int sqlPort, const char* sqlUser, 
              const char* sqlPwd, const char* dbName, 
              int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize);
    ~webServer();
    void start();
private:
    bool initSocket_(); // 初始化socket
    void initEventMode_(int trigMode); // 初始化事件模式
    void addClient_(int fd, sockaddr_in addr); // 添加客户端
    void dealListen_(); // 处理监听事件
    void dealWrite_(HttpConn* client); // 处理写事件
    void dealRead_(HttpConn* client); // 处理读事件
    void sendError_(int fd, const char*info); // 发送错误信息
    void extTimer_(HttpConn* client); // 延长定时器
    void closeConn_(HttpConn* client); // 关闭连接
    void onRead_(HttpConn* client); // 读事件
    void onWrite_(HttpConn* client); // 写事件
    void onProcess(HttpConn* client);

    static const int MAX_FD = 65536; // 最大文件描述符
    static int setFdNonblock(int fd); // 设置非阻塞


    int port_; // 端口
    bool openLinger_; // 是否开启优雅关闭
    int timeoutMS_; // 超时时间
    bool isClose_; // 是否关闭
    int listenFd_; // 监听文件描述符
    char* srcDir_; // 资源目录
    uint32_t listenEvent_; // 监听事件
    uint32_t connEvent_; // 连接事件

    std::unique_ptr<HeapTimer> timer_; // 定时器
    std::unique_ptr<ThreadPool> threadpool_; // 线程池
    std::unique_ptr<Epoller> epoller_; // epoll
    std::unordered_map<int, HttpConn> users_; // 用户
};

#endif // WEBSERVER_H