#include "webserver.h"

webServer::webServer(int port, int trigMode, int timeoutMS, 
                     bool OptLinger, int sqlPort, const char* sqlUser, 
                     const char* sqlPwd, const char* dbName, 
                     int connPoolNum, int threadNum, int openLog, 
                     int logLevel, int logQueSize) 
    : port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
      timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    srcDir_ = getcwd(nullptr, 256); // 获取当前工作目录
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16); // 拼接资源目录
    HttpConn::userCount = 0;  // 初始化用户数量
    HttpConn::srcDir = srcDir_; // 设置资源目录

    initEventMode_(trigMode);  // 初始化事件模式
    //  初始化数据库连接池
    if (!SqlConnPool::Instance()->init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum)) {
        LOG_ERROR("SqlConnPool init error");
        exit(1);
    }
    if(initSocket_()) { // 初始化socket
        isClose_ = false;
        LOG_INFO("Init socket success");
    } else {
        LOG_ERROR("Init socket error");
        isClose_ = true;
    }

    // 是否打开日志标志
    if(openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) {
            LOG_ERROR("Server init error");
            exit(1);
        } else {
            LOG_INFO("Server init success");
        }
    }
}

webServer::~webServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->closePool();
}

void webServer::initEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;  // 关闭检测socket连接的关闭
    // 连接事件时关注对端关闭连接的情况，并且该事件是一次性的，触发后需要重新设置。
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; 
    switch (trigMode) {
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
        default:
            listenEvent_ = EPOLLRDHUP;
            connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
            break;
    }
    // 表示HttpConn 是否开启ET模式
    HttpConn::isET = (connEvent_ & EPOLLET); 
}

// 
void webServer::start(){
    int timeMS = -1; // epoll_wait() 的超时时间为-1，表示永久阻塞
    if(!isClose_) {
        LOG_INFO("Server start");
    }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->getNextTick(); // 获取定时器的超时时间
        }
        int eventCnt = epoller_->wait(timeMS); // 等待事件数目
        for(int i = 0; i < eventCnt; i++) {
            int fd = epoller_->getEventFd(i); // 获取事件的文件描述符
            uint32_t events = epoller_->getEvents(i); // 获取事件
            if(fd == listenFd_) {
                dealListen_(); // 处理监听事件
            } else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                closeConn_(&users_[fd]); // 关闭连接
            } else if(events & EPOLLIN) {
                if(users_.count(fd) > 0) {
                    onRead_(&users_[fd]); // 读事件
                } else {
                    LOG_ERROR("Error: fd no exist");
                }
            } else if(events & EPOLLOUT) {
                if(users_.count(fd) > 0) {
                    onWrite_(&users_[fd]); // 写事件
                } else {
                    LOG_ERROR("Error: fd no exist");
                }
            } else {
                LOG_ERROR("Error: something else");
            }
        }
    }
}

// 发送错误信息到客户端，info为错误信息
void webServer::sendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void webServer::closeConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    epoller_->delFd(client->getFd());
    client->close();
}

void webServer::addClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&webServer::closeConn_, this, &users_[fd]));
    }
    epoller_->addFd(fd, EPOLLIN | connEvent_);
    setFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].getFd());
}

void webServer::dealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &len);
        if(fd <= 0) {
            return;
        } else if(HttpConn::userCount >= MAX_FD) {
            sendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        addClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

void webServer::onRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN) {
        closeConn_(client);
        return;
    }
    onProcess(client);
}

